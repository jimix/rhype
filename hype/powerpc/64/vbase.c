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

/* Implementation of virtualized base memory */

#include <config.h>
#include <types.h>
#include <cpu_thread.h>
#include <pmm.h>
#include <vregs.h>
#include <xh.h>
#include <bitops.h>
#include <hype_control_area.h>
#include <thread_control_area.h>
#include <objalloc.h>
#include <vm_class.h>
#include <vm_class_inlines.h>
#include <vdec.h>
#include <vm.h>
#include <bitmap.h>


#define FORCE_4K_PAGES 1

sval
vbase_config(struct cpu_thread* thr, uval vbase, uval size)
{
	uval i = 0;
	struct vstate *v;
	thr->vbo = vbase;
	thr->vbl = size;

	struct vm_class *vmc;
	thr->vmc_vregs = vmc_create_vregs();


	thr->vregs = (struct vregs*)thr->vmc_vregs->vmc_data;
	memset(thr->vregs, 0, sizeof(typeof(*thr->vregs)));
	thr->vregs->v_pir = mfpir();
	v = &thr->vstate;
	v->thread_mode = VSTATE_KERN_MODE;


	vmc = vmc_create_linear(0, 0, 1ULL << VM_CLASS_SPACE_SIZE_BITS, 0);
	thr->cpu->os->vmc_kernel[0] = vmc;


	for (i = 0 ; i < NUM_MAP_CLASSES; ++i) {
		thr->vregs->v_active_cls[i] = INVALID_CLASS;
	}

	partition_set_dec(thr, -1);

	return 0;
}



uval
xh_syscall(uval a1, uval a2, uval a3, uval a4, uval a5, uval a6,
	   uval a7, uval a8, uval a9, uval a10);

typedef	sval (*hcall_fn_t)(struct cpu_thread* thr, uval a2, uval a3, uval a4,
			   uval a5, uval a6, uval a7, uval a8,
			   uval a9, uval a10);


sval xh_kern_slb(struct cpu_thread* thread, uval addr,
		 struct vexc_save_regs *vr);
sval xh_kern_pgflt(struct cpu_thread* thread, uval addr,
		   struct vexc_save_regs *vr);

sval
htab_purge_vsid(struct cpu_thread *thr, uval vsid, uval num_vsids)
{
	uval end = thr->cpu->os->htab.num_ptes;
	uval i = 0;
	union pte *ht = (union pte*)GET_HTAB(thr->cpu->os);
	for (; i < end; ++i) {
		/* vsid is 52 bits, avpn has 57 */
		uval pte_vsid = ht[i].bits.avpn >> 5;
		if (ht[i].bits.v == 0 || pte_vsid - vsid >= num_vsids) {
			continue;
		}

		pte_lock(&ht[i]);
		if (ht[i].bits.v == 0 || pte_vsid - vsid >= num_vsids) {
			pte_unlock(&ht[i]);
			continue;
		}

		pte_invalidate(&thr->cpu->os->htab, &ht[i]);
		pte_unlock(&ht[i]);
		ptesync();
		do_tlbie(&ht[i], i);
	}
	return H_Success;
}


static void
record_eviction(union pte *entry)
{
	(void)entry;
}

sval
insert_ea_map(struct cpu_thread *thr, uval vsid, uval ea, uval valid,
	      union ptel entry)
{
	int j = 0;
	uval lp = LOG_PGSIZE;  /* FIXME: get large page size */
	uval vaddr = (vsid << LOG_SEGMENT_SIZE) | (ea & (SEGMENT_SIZE - 1));
	uval log_ht_size = bit_log2(thr->cpu->os->htab.num_ptes)+ LOG_PTE_SIZE;
	uval pteg = NUM_PTES_IN_PTEG * get_pteg(vaddr, vsid, lp, log_ht_size);

	union pte *ht = (union pte *)GET_HTAB(thr->cpu->os);
	union pte pte;

	pte.words.vsidWord = 0;
	pte.words.rpnWord = entry.word;

	pte.bits.avpn = (vsid << 5) | VADDR_TO_API(vaddr);
	pte.bits.v = valid;

	union pte *target = &ht[pteg];
	int empty = NUM_PTES_IN_PTEG;
	uval remove = 0;

redo_search:

	for (; j < NUM_PTES_IN_PTEG; ++j, ++target) {
		if (target->bits.avpn == pte.bits.avpn) {
			empty = j;
			remove = 1;

			break;
		}
		if (j < empty && target->bits.v == 0) {
			empty = j;
		}
	}

	if (empty == NUM_PTES_IN_PTEG) {
		assert(empty != NUM_PTES_IN_PTEG, "Full htab\n");
		record_eviction(&ht[pteg]);
		goto redo_search;
	}


	target = &ht[pteg + empty];
	pte_lock(target);

	/* Things may have changed since we got the lock */
	if (target->bits.v && target->bits.avpn != pte.bits.avpn)
		goto redo_search;


	if (remove) {
		pte_invalidate(&thr->cpu->os->htab, target);
		ptesync();
		do_tlbie(target, pteg + empty);
	}

//	hprintf("insert: ea: 0x%lx vsid: 0x%lx -> idx: 0x%lx\n",
//		ea, vsid, j + pteg);
	if (pte.bits.v) {
		pte.bits.lock = 1; /* Make sure new entry is still locked */
		pte_insert(&thr->cpu->os->htab, empty + pteg,
			   pte.words.vsidWord, pte.words.rpnWord, ea);
	}
	pte_unlock(target);

	return H_Success;
}


sval
xh_kern_pgflt(struct cpu_thread* thr, uval type, struct vexc_save_regs *vr)
{
	struct vm_class *vmc = NULL;
	uval orig_addr;
	struct thread_control_area *tca = get_tca();

	if (type == 1) {
		orig_addr = mfdar();
	} else {
		orig_addr = tca->srr0;
	}


	if (thr->vstate.thread_mode & VSTATE_KERN_MODE) {
		vmc = find_kernel_vmc(thr, orig_addr);
	}

	if (!vmc) {
		vmc = find_app_vmc(thr, orig_addr);
	}

	if (!vmc) {
		hprintf("No vm_class for 0x%lx\n", orig_addr);
		breakpoint();

		return insert_debug_exception(thr, V_DEBUG_MEM_FAULT);
	}

	uval addr = ALIGN_DOWN(orig_addr, PGSIZE);
	union ptel pte = { .word = 0 };
	uval la = vmc_xlate(vmc, addr, &pte);
	uval ra;
	uval vsid;

	if (la == INVALID_LOGICAL_ADDRESS) {
		/* If logical address is invalid, and pte is non-zero, then
		 * pte contains physical address
		 */
		if (pte.word == 0) {
			goto reflect;
		}

		ra = pte.bits.rpn << LOG_PGSIZE;
	} else {
		ra = logical_to_physical_address(thr->cpu->os, la, PGSIZE);
	}

	vsid = vmc_class_vsid(thr, vmc, addr);

	pte.bits.rpn = ra >> LOG_PGSIZE;

	sval ret = insert_ea_map(thr, vsid, addr, 1, pte);
	if (ret == H_Success) {
		++vmc->vmc_num_ptes;
		return vr->reg_gprs[3];
	}

reflect:

//	hprintf("reflect fault:   0x%lx 0x%08lx %lx\n",
//		orig_addr, mfdsisr(), tca->srr0);

	if (thr->vregs->debug & V_DEBUG_FLAG) {
		return insert_debug_exception(thr, V_DEBUG_MEM_FAULT);
	}

	thr->vregs->v_dar = orig_addr;
	thr->vregs->v_dsisr = mfdsisr();


	assert(thr->vregs->exception_vectors[EXC_V_PGFLT],
	       "no pgflt vector defined\n");

	return insert_exception(thr, EXC_V_PGFLT);
}


sval
xh_kern_slb(struct cpu_thread* thread, uval type, struct vexc_save_regs *vr)
{
	struct vm_class *vmc = NULL;
	struct thread_control_area *tca = get_tca();
	uval addr;

	if (type == 1) {
		addr = mfdar();
	} else {
		addr = tca->srr0;
	}

	uval seg_base = ALIGN_DOWN(addr, SEGMENT_SIZE);
	uval lp = LOG_PGSIZE;  /* FIXME: get large page size */
	uval l = 1;
	uval spot;

	if (thread->vstate.thread_mode & VSTATE_KERN_MODE) {
		vmc = find_kernel_vmc(thread, addr);
	}

	if (!vmc) {
		vmc = find_app_vmc(thread, addr);
	}

	if (!vmc) {
		hprintf("No vm_class for 0x%lx\n", addr);
		breakpoint();
		return insert_debug_exception(thread, V_DEBUG_MEM_FAULT);
	}


	uval vsid = vmc_class_vsid(thread, vmc, addr);

#ifdef FORCE_4K_PAGES
	lp = 12;
	l = 0;
	spot = slb_insert(seg_base, 0, 0, 1, vsid, thread->slb_entries);
#else
	spot = slb_insert(ea, 1, SELECT_LG, 1, vsid, thread->slb_entries);
#endif

	return vr->reg_gprs[3];
}

uval
xh_syscall(uval a1, uval a2, uval a3, uval a4, uval a5, uval a6,
	   uval a7, uval a8, uval a9, uval a10)
{
	struct thread_control_area* tca = (struct thread_control_area*)mfr13();
	struct cpu_thread* thread = tca->active_thread;
	hcall_fn_t hcall_fn;
	const hcall_vec_t* vec = (const hcall_vec_t*)hca.hcall_vector;
	thread->return_args = tca->save_area;

	a1 >>= 2;

	if (a1 >= hca.hcall_vector_len &&
	    a1 - 0x1800 < hca.hcall_6000_vector_len) {
		vec = (const hcall_vec_t*)hca.hcall_6000_vector;
		a1 -= 0x1800;
	}

	hcall_fn = *(const hcall_fn_t*)&vec[a1];

	uval ret;
	ret = hcall_fn(thread, a2, a3, a4, a5, a6, a7, a8, a9, a10);
	tca->save_area->reg_gprs[3] = ret;
	return ret;
}

