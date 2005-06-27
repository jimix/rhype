/*
 * Copyright (C) 2005 Jimi Xenidis <jimix@watson.ibm.com>, IBM Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * $Id$
 */

#include <config.h>
#include <types.h>
#include <cpu_thread.h>
#include <pmm.h>
#include <vregs.h>
#include <xh.h>
#include <bitops.h>
#include <objalloc.h>
#include <vm_class.h>
#include <vm_class_inlines.h>
#include <cpu_thread_inlines.h>
#include <htab_inlines.h>
#include <gen.h>
#include <thread_control_area.h>
#define FORCE_4K_PAGES 1


struct vregs_vm_class
{
	struct vm_class vvc_vm_class;
	union pte*	protected_pte;
};


void
__thread_set_MER(struct cpu_thread *thr)
{
	struct vregs_vm_class *vmc = PARENT_OBJ(typeof(*vmc), vvc_vm_class,
						thr->vmc_vregs);

	union pte * p = vmc->protected_pte;

	union pte pte = *p;

	if (is_MER_active(thr)) {
		pte.bits.pp0 = PP_RWRx & 1;
		pte.bits.pp1 = PP_RWRx >> 1;
	} else {
		pte.bits.pp0 = PP_RWRW & 1;
		pte.bits.pp1 = PP_RWRW >> 1;
	}

	if (pte.words.rpnWord != p->words.rpnWord) {
		uval vsid = vmc_class_vsid(thr, &vmc->vvc_vm_class,
					   VREG_PROTECTED_BASE);

		uval vpn = vpn_from_vsid_ea(vsid, VREG_PROTECTED_BASE);
		htab_entry_modify(p, vpn, &pte);
	}
}


static uval
vmc_vregs_xlate(struct vm_class *vmc, uval eaddr, union ptel *pte)
{
	(void)eaddr;

	pte->word = 0;
	pte->bits.rpn = vmc->vmc_data >> LOG_PGSIZE;
	pte->bits.m = 1;
	pte->bits.pp0 = PP_RWRW & 1;
	pte->bits.pp1 = PP_RWRW >> 1;

	return INVALID_LOGICAL_ADDRESS;
}

/* Extract bits of word, start/stop indexing uses Book I/III conventions
 * (i.e. bit 0 is on the left)
 */
static inline
uval extract_bits(uval val, uval start, uval numbits)
{
	return (val >> (32 - (start + numbits))) & ((1 << numbits) - 1);
}

static uval
vmc_vregs_exception(struct vm_class *vmc, struct cpu_thread *thr,
		    struct vexc_save_regs *vr, uval eaddr)
{
	uval vsid = vmc_class_vsid(thr, vmc, eaddr);
	union ptel pte;
	pte.word = 0;
	pte.bits.rpn = vmc->vmc_data >> LOG_PGSIZE;
	pte.bits.m = 1;
	pte.bits.pp0 = PP_RWRW & 1;
	pte.bits.pp1 = PP_RWRW >> 1;

	if (eaddr >= VREG_BASE) {
		sval ret = insert_ea_map(thr, vsid, eaddr, 1, pte);
		assert(ret == H_Success,
		       "Failed on vregs area PTE insertion\n");
		return vr->reg_gprs[3];
	}

	if (eaddr < VREG_PROTECTED_BASE) {
		thr->vregs->v_dar = eaddr;
		thr->vregs->v_dsisr = mfdsisr();
		return insert_exception(thr, EXC_V_PGFLT);
	}

	uval offset = eaddr - VREG_PROTECTED_BASE;
	assert(offset == V_MSR, "Store to non-msr field\n");


	uval32 ins = fetch_instruction(thr, get_tca()->srr0);

	uval opcode = extract_bits(ins, 0, 6);
	uval update_bit = extract_bits(ins, 31, 1);

	assert(opcode == 62 && update_bit == 0, "bad store op code\n");

	uval rs = extract_bits(ins, 6, 5);
	uval ds = extract_bits(ins, 16, 14) << 2;

	assert(ds == (eaddr & 0xffff), "eaddr doesn't match ds\n");
	assert(rs < 14, "assuming rs is r0 .. r13\n");

	uval val = vr->reg_gprs[rs];

	set_v_msr(thr, val);

	get_tca()->srr0 += sizeof(uval32);

	return vr->reg_gprs[3];
}



struct vm_class_ops vmc_vregs_ops =
{
	.vmc_xlate = vmc_vregs_xlate,
	.vmc_exception = vmc_vregs_exception,
};



struct vm_class*
vmc_create_vregs(struct cpu_thread *thr)
{
	struct vregs_vm_class* vmc = halloc(sizeof(*vmc));
	if (!vmc) return NULL;

	vmc_init(&vmc->vvc_vm_class, ~0ULL, VREG_PROTECTED_BASE,
		 2 * PGSIZE, &vmc_vregs_ops);
	vmc->vvc_vm_class.vmc_data = (uval)halloc(PGSIZE);



	uval vsid = vmc_class_vsid(thr, &vmc->vvc_vm_class,
				   VREG_PROTECTED_BASE);

	uval vpn = vpn_from_vsid_ea(vsid, VREG_PROTECTED_BASE);
	union pte pte;
	pte.words.rpnWord = 0;
	pte.words.vsidWord = 0;
	pte.bits.rpn = vmc->vvc_vm_class.vmc_data >> LOG_PGSIZE;
	pte.bits.m = 1;
	pte.bits.pp0 = PP_RWRW & 1;
	pte.bits.pp1 = PP_RWRW >> 1;
	pte.bits.avpn = (vsid << 5) | VADDR_TO_API(vpn << LOG_PGSIZE);
	pte.bits.v = 1;

	/* We don't unlock the PTE because we want to ensure nobody
	 * ever touches it
	 */
	pte.bits.bolted = 1;
	pte.bits.lock = 1;

	union pte * target = locate_clear_lock_target(thr, vpn);

	vmc->protected_pte = target;

	htab_entry_modify(target, vpn, &pte);


	return &vmc->vvc_vm_class;
}


