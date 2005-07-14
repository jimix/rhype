/*
 * Copyright (C) 2005 Michal Ostrowski <mostrows@watson.ibm.com>, IBM Corp.
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

/* Implementation of virtualized environment */

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
#include <cpu_thread_inlines.h>
#include <hcounters.h>
#include <counter.h>

sval xh_kern_prog_ex(struct cpu_thread* thread, uval addr);

static void
enter_kernel(struct cpu_thread *thread)
{
	uval i = 0;
	struct slb_cache *k = &thread->vstate.kern_slb_cache;
	struct slb_cache* c = &thread->slbcache;
	uval map = k->used_map;
	for_each_bit(map, i) {
		if (c->entries[i].bits.v) {
			inval_slb_cache_entry(i, k);
		} else {
			set_slb_entry(i, c, &k->entries[i]);
		}
	}
	set_vmode_bit(thread, VSTATE_KERN_MODE);
	hit_counter(HCNT_KERNEL_ENTER);

}

static void
exit_kernel(struct cpu_thread *thread)
{
	uval i = 0;
	uval map = thread->vstate.kern_slb_cache.used_map;
	for_each_bit(map ,i) {
		inval_slb_entry(i, &thread->slbcache);
	}
	clear_vmode_bit(thread, VSTATE_KERN_MODE);
	hit_counter(HCNT_KERNEL_EXIT);
}

inline void
set_v_msr(struct cpu_thread* thr, uval val)
{
	if ((val ^ thr->vregs->v_msr) & MSR_PR) {
		if (val & MSR_PR) {
			exit_kernel(thr);
		} else {
			enter_kernel(thr);
		}
	}

	uval ee = val & MSR_EE;
	thr->vregs->v_msr = (val & ~(MSR_HV|(MSR_SF>>2))) | MSR_AM;

	if (!ee) {
		clear_vmode_bit(thr, VSTATE_FORCE_EXCEPTION);
		return;
	}

	if (test_vmode_bit(thr, VSTATE_PENDING)) {
		set_vmode_bit(thr, VSTATE_FORCE_EXCEPTION);
	}
}


static inline void
mtgpr(struct cpu_thread* thr, uval gpr, uval val)
{
	switch (gpr) {
	case 14 ... 31:
		thr->reg_gprs[gpr] = val;
		break;
	case 0 ... 13:
		get_tca()->save_area->reg_gprs[gpr] = val;
		break;
	}
}

static inline uval
mfgpr(struct cpu_thread* thr, uval gpr)
{
	uval val = 0;
	switch (gpr) {
	case 14 ... 31:
		val = thr->reg_gprs[gpr];
		break;
	case 0 ... 13:
		val = get_tca()->save_area->reg_gprs[gpr];
		break;
	}
	return val;

}

/* Extract bits of word, start/stop indexing uses Book I/III conventions
 * (i.e. bit 0 is on the left)
 */
static inline
uval extract_bits(uval val, uval start, uval numbits)
{
	return (val >> (32 - (start + numbits))) & ((1 << numbits) - 1);
}

static sval
decode_spr_ins(struct cpu_thread* thr, uval addr, uval32 ins)
{
	struct thread_control_area *tca = get_tca();
	uval id = thr->vregs->active_vsave;
	struct vexc_save_regs *vr = &thr->vregs->vexc_save[id];
	sval ret = -1;
	uval opcode = extract_bits(ins, 0, 6);
	uval type = extract_bits(ins, 21, 10);
	uval spr_0_4 = extract_bits(ins, 16, 5);
	uval spr_5_9 = extract_bits(ins, 11, 5);
	uval gpr  = extract_bits(ins, 6, 5);
	uval spr = (spr_0_4 << 5) | spr_5_9;

	sync_from_dec();

	/* mfmsr */
	if (opcode == 31 && type == 83) {
		hit_counter(HCNT_PRIV_MFMSR);
		//hprintf("mfmsr r%ld at 0x%lx\n",gpr, addr);
		mtgpr(thr, gpr, thr->vregs->v_msr);
		tca->srr0 += sizeof(uval32);
		return 0;
	}

	/* mtmsrd */
	if (opcode == 31 && (type == 178 || type == 146)) {
		hit_counter(HCNT_PRIV_MTMSRD);

		uval64 val = mfgpr(thr, gpr);
		//hprintf("mtmsrd r%ld <- 0x%llx at 0x%lx\n", gpr, val, addr);

		uval64 chg_mask = ~0ULL;
		uval l = extract_bits(ins, 15, 1);
		if (type == 146) {
			// mtmsr , 32-bits
			chg_mask = 0xffffffff;
		}

		if (l == 1) {
			chg_mask = (MSR_EE | MSR_RI);
		}

		/* These are the only bits we can change here */
		val = (val & chg_mask) | (thr->vregs->v_msr & ~chg_mask);

		set_v_msr(thr, val);
		val = thr->vregs->v_msr;
		val |= V_LPAR_MSR_ON;
		val &= ~V_LPAR_MSR_OFF;
		tca->srr1 = val;
		tca->srr0 += sizeof(uval32);
		return 0;
	}


	hit_counter(HCNT_PRIV_OTHER);

	/* mfspr */
#define SET_GPR(label, src)					\
	case label: mtgpr(thr, gpr, src); break;

	if (opcode == 31 && type == 339) {
		ret = 0;
		switch (spr) {
			SET_GPR(SPRN_SRR0, vr->v_srr0);
			SET_GPR(SPRN_SRR1, vr->v_srr1);
			SET_GPR(SPRN_PVR, mfpvr());
			SET_GPR(SPRN_PIR, mfpir());
			SET_GPR(SPRN_DEC, thr->vregs->v_dec);

		case SPRN_DSISR:
		case SPRN_DAR:
			mtgpr(thr, gpr, 0);
			break;
		case SPRN_HID0:
		case SPRN_HID1:
		case SPRN_HID4:
		case SPRN_HID5:
			mtgpr(thr, gpr, 0xdeadbeeffeedfaceULL);
			break;

		default:
			ret = -1;
			break;
		}

		if (ret != -1) {
			tca->srr0 += sizeof(uval32);
			return ret;
		}

	}

#define SET_VREG(label, field)						\
	case label: thr->vregs->field = mfgpr(thr, gpr); break;

	/* mtspr */
	if (opcode == 31 && type == 467) {
		ret = 0;
		switch (spr) {
			SET_VREG(SPRN_SPRG0, v_sprg0);
			SET_VREG(SPRN_SPRG1, v_sprg1);
			SET_VREG(SPRN_SPRG2, v_sprg2);
			SET_VREG(SPRN_SPRG3, v_sprg3);
		case SPRN_DEC:
			partition_set_dec(thr, mfgpr(thr, gpr));
			thr->vregs->v_dec = mfgpr(thr, gpr);
			break;
		case SPRN_SRR0:
			vr->v_srr0 = mfgpr(thr, gpr);
			break;
		case SPRN_SRR1:
			vr->v_srr1 = mfgpr(thr, gpr);
			break;

		case SPRN_DSISR:
		case SPRN_DAR:
			break;
		default:
			ret = -1;
			break;
		}

		if (ret != -1) {
			tca->srr0 += sizeof(uval32);
			return ret;
		}

	}

	/* rfid */
	if (opcode == 19 && type == 18) {

		uval val = vr->v_srr1;

		set_v_msr(thr, val);
		val |= V_LPAR_MSR_ON;
		val &= ~V_LPAR_MSR_OFF;
		tca->srr1 = val;
		tca->srr0 = vr->v_srr0;
		hprintf("rfid: %lx -> %lx\n",addr, vr->v_srr0);

		return 0;
	}

	if (ret == -1) {
		hprintf("Decode instruction: %ld %ld %ld %ld\n",
			opcode, type, spr, gpr);
	}
	return ret;
}


uval32
fetch_instruction(struct cpu_thread *thread, uval addr)
{
	uval32 ins = 0;
	struct vm_class *vmc = find_kernel_vmc(thread, addr);
	if (!vmc) goto abort;

	union ptel pte;
	uval laddr = vmc_xlate(vmc, addr, &pte);
	if (laddr == INVALID_LOGICAL_ADDRESS) goto abort;

	uval pa = logical_to_physical_address(thread->cpu->os, laddr,
					      sizeof(uval32));
	if (pa == INVALID_PHYSICAL_ADDRESS) goto abort;

	ins = *(uval32*)pa;

abort:
	return ins;

}


sval
xh_kern_prog_ex(struct cpu_thread* thread, uval addr)
{
	uval32 ins;
	struct thread_control_area *tca = get_tca();

	sval ret = 0;
	uval srr1 = tca->srr1;

	if (! (srr1 & (1ULL << (63-45)))) goto abort;

	ins = fetch_instruction(thread, addr);
	if (ins == 0) goto abort;

	ret = decode_spr_ins(thread, addr, ins);
	if (ret == 0) return -1;
abort:
	return insert_exception(thread, EXC_V_DEBUG);
}


sval
deliver_async_exception(struct cpu_thread *thread)
{
	sval ret = -1;

	if (test_vmode_bit(thread, VSTATE_PENDING_EXT)) {
		/* Ordering is important so as to handle MER emulation.
		 * Clear the PENDING bits only after the new msr has been set
		 */
		ret = insert_exception(thread, EXC_V_EXT);
		clear_vmode_pending(thread, VSTATE_PENDING_EXT);
	} else if (test_vmode_bit(thread, VSTATE_PENDING_DEC)) {
		/* Ordering is important so as to handle MER emulation.
		 * Clear the PENDING bits only after the new msr has been set
		 */
		ret = insert_exception(thread, EXC_V_DEC);
		hit_counter(HCNT_DEC_INT);
		clear_vmode_pending(thread, VSTATE_PENDING_DEC);
	}

	return ret;
}

sval
insert_exception(struct cpu_thread *thread, uval exnum)
{
	struct thread_control_area *tca = get_tca();
	struct vregs* vregs = tca->vregs;
	struct vexc_save_regs* vr = tca->save_area;

	if (!thread->vregs->exception_vectors[exnum])
		return vregs->active_vsave;

	hit_counter_cond(HCNT_SYSCALL, exnum == EXC_V_SYSCALL);

//	hprintf("%s from 0x%016lx %lx\n", __func__, tca->srr0, exnum);

	vr->v_srr0 = tca->srr0;

	assert(tca->srr0 != 0xf000000000000000, "bad reflection");


	/* Provide accurate trap bits */
	vr->v_srr1 = vregs->v_msr | (MSR_TRAP_BITS & tca->srr1);

	/* enforce HV, EE, PR off, AM on */
	uval vmsr = vregs->v_msr;

	vmsr &= ~(MSR_EE|MSR_HV|MSR_PR|MSR_IR|MSR_DR|MSR_FE0|
		  MSR_FE1|MSR_BE|MSR_FP|MSR_PMM|MSR_SE|MSR_BE);
	vmsr |= MSR_SF;
//	hprintf("v_msr: 0x%016lx v_ex_msr: 0x%016lx %ld\n",
//		vregs->v_msr, vmsr, exnum);
	uval64 msr = vregs->v_ex_msr;

	if (exnum == EXC_V_DEBUG) {
		vmsr |= MSR_FP;
		msr |= MSR_FP;
	}

	set_v_msr(thread, vmsr);

	msr |= V_LPAR_MSR_ON;
	msr &= ~V_LPAR_MSR_OFF;
	tca->srr1 = msr;
	tca->srr0 = vregs->exception_vectors[exnum];

	/* Make vregs->v_dec match current dec value */
	sync_from_dec();


	vr->prev_vsave = vregs->active_vsave;
	vregs->active_vsave = (vr - &vregs->vexc_save[0]);

	return vregs->active_vsave;
}


sval
insert_debug_exception(struct cpu_thread *thread, uval dbgflag)
{
	struct thread_control_area *tca = get_tca();

	if (thread->vregs->debug & V_DEBUG_FLAG) {

		thread->vregs->debug |= dbgflag;

		/* Increment to the next instruction */
		tca->srr0 += sizeof(uval32);

		return thread->vregs->active_vsave;
	}

	return insert_exception(thread, EXC_V_DEBUG);

}

