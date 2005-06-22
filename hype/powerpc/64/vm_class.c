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
#include <counter.h>

#define FORCE_4K_PAGES 1

uval
vmc_init(struct vm_class *vmc, uval id, uval base, uval size,
	 struct vm_class_ops *ops)
{
	vmc->vmc_hash.he_key = id;
	vmc->vmc_base_ea = base;
	vmc->vmc_size = size;
	vmc->vmc_ops = ops;
	vmc->vmc_active_count = 0;
	vmc->vmc_num_ptes = 0;
	memset(&vmc->vmc_slb_cache[0], 0xff, sizeof(vmc->vmc_slb_cache));
	return 0;
}

sval
vmc_destroy(struct cpu_thread *thr, struct vm_class *vmc)
{
	vmc_deactivate(thr, vmc);

	if (vmc->vmc_num_ptes) {
		htab_purge_vsid(thr,
				vmc_class_vsid(thr, vmc, vmc->vmc_base_ea),
				1ULL << VM_CLASS_BITS);
	}

	lock_acquire(&thr->cpu->os->po_mutex);

	if (vmc->vmc_id < NUM_KERNEL_VMC) {
		thr->cpu->os->vmc_kernel[vmc->vmc_id] = NULL;
	} else {
		ht_remove(&thr->cpu->os->vmc_hash, vmc->vmc_id);
	}

	lock_release(&thr->cpu->os->po_mutex);

	if (vmc->vmc_ops->vmc_dealloc) {
		vmc->vmc_ops->vmc_dealloc(vmc);
	} else {
		hfree(vmc, sizeof(*vmc));
	}
	return H_Success;
}


static uval
vmc_linear_xlate(struct vm_class *vmc, uval eaddr, union ptel *pte)
{
	uval la = (eaddr - vmc->vmc_base_ea) + vmc->vmc_data;
	pte->word = 0;
	pte->bits.rpn = la >> LOG_PGSIZE;
	pte->bits.m = 1;
	pte->bits.pp0 = PP_RWRW & 1UL;
	pte->bits.pp1 = (PP_RWRW >> 1) & 3UL;

	if (eaddr == 0xc000000004000000ULL) {
		breakpoint();
	}
	return la;
}

struct vm_class_ops vmc_linear_ops =
{
	.vmc_xlate = vmc_linear_xlate,
};


struct vm_class*
vmc_create_linear(uval id, uval base, uval size, uval offset)
{
	struct vm_class* vmc = halloc(sizeof(*vmc));
	if (!vmc) return NULL;

	vmc_init(vmc, id, base, size, &vmc_linear_ops);
	vmc->vmc_data = offset;
	return vmc;
}

struct vm_class*
find_kernel_vmc(struct cpu_thread* thr, uval eaddr)
{
	struct os *os = thr->cpu->os;
	int i;
	struct vm_class *vmc;
	for (i = 0; i < NUM_KERNEL_VMC; ++i) {
		vmc = os->vmc_kernel[i];
		if (vmc == NULL) continue;
		if (vmc->vmc_base_ea > eaddr) continue;
		if (vmc->vmc_base_ea + vmc->vmc_size < eaddr) continue;
		return vmc;
	}

	if (eaddr > thr->vmc_vregs->vmc_base_ea) {
		return thr->vmc_vregs;
	}

	return NULL;
}

struct vm_class*
find_app_vmc(struct cpu_thread* thr, uval eaddr)
{
	int i;
	struct vm_class *vmc;
	for (i = 0; i < NUM_MAP_CLASSES; ++i) {
		vmc = thr->vstate.active_cls[i];
		if (vmc == NULL) continue;
		if (vmc->vmc_base_ea > eaddr) continue;
		if (vmc->vmc_base_ea + vmc->vmc_size < eaddr) continue;
		return vmc;
	}
	return NULL;
}


void
vmc_deactivate(struct cpu_thread *thread, struct vm_class *vmc)
{
	uval i = 0;
	hit_counter(HCNT_CLASS_DEACTIVATE);

	/* The vmc is responsible for all vsids in its range, even
	 * if vmc_base_ea is not at the vm class boundary
	 */

	/* clear the cache */
	memset(&vmc->vmc_slb_cache[0], 0xff, sizeof(vmc->vmc_slb_cache));

	for (; i < SWSLB_SR_NUM; ++i) {
		union slb_entry *se = &thread->slb_entries[i];
		if (!se->bits.v) continue;

		if (vsid_class_id(se->bits.vsid) != vmc->vmc_id)
			continue;

		/* remember the entry for the cache */
		vmc_cache_slbe(vmc, se->bits.vsid);

		inval_slb_entry(i, thread->slb_entries);
	}
	vmc->vmc_slb_entries = 0;
	vmc_put(vmc);

	/* preserve vmc_slb_cache, it tells us which slb entries we should
	 * create to restore things when this vmc is activated */
}

void
vmc_activate(struct cpu_thread *thread, struct vm_class *vmc)
{
	uval idx = 0;
	uval i = 0;
	hit_counter(HCNT_CLASS_ACTIVATE);
	vmc_get(vmc);
	return;

	for (; i < SWSLB_SR_NUM; ++i) {
		union slb_entry *se = &thread->slb_entries[i];

		if (se->bits.v == 1) continue;

		while (vmc->vmc_slb_cache[idx] == VMC_UNUSED_SLB_ENTRY &&
		       idx++ < VMC_SLB_CACHE_SIZE);

		/* nothing else to restore */
		if (idx == VMC_SLB_CACHE_SIZE) break;

		uval ea = (vmc->vmc_slb_cache[idx] * SEGMENT_SIZE)
			+ ALIGN_DOWN(vmc->vmc_base_ea, VM_CLASS_SIZE);

		if (ea < vmc->vmc_base_ea) {
			assert(vmc->vmc_base_ea - ea < SEGMENT_SIZE,
			       "invalid index in cache");
			ea = vmc->vmc_base_ea;
		}

		/* insert the actual slb entry */
		uval vsid = vmc_class_vsid(thread, vmc, ea);
		uval lp = LOG_PGSIZE;  /* FIXME: get large page size */
		uval l = 1;

#ifdef FORCE_4K_PAGES
		lp = 12;
		l = 0;
		slb_insert_to_slot(i, ea, 0, 0, 1, vsid, thread->slb_entries);
#else
		slb_insert_to_slot(i, ea, 1, SELECT_LG, 1, vsid,
				   thread->slb_entries);
#endif

		++idx;

	}

}


void
vmc_enter_kernel(struct cpu_thread *thread)
{
	uval i = 0;
	vmc_activate(thread, thread->vmc_vregs);
	for (; i < NUM_KERNEL_VMC; ++i) {
		if (!thread->cpu->os->vmc_kernel[i]) continue;
		vmc_activate(thread, thread->cpu->os->vmc_kernel[i]);
	}
}

void
vmc_exit_kernel(struct cpu_thread *thread)
{
	uval i = 0;
	for (; i < NUM_KERNEL_VMC; ++i) {
		if (!thread->cpu->os->vmc_kernel[i]) continue;
		vmc_deactivate(thread, thread->cpu->os->vmc_kernel[i]);
	}
	vmc_deactivate(thread, thread->vmc_vregs);

}


static inline sval
vmc_reflect_enter(struct vm_class *vmc, struct cpu_thread *thr,
		  uval ea, uval linux_pte)
{
	union linux_pte lpte = { .word = linux_pte };


	uval lp = LOG_PGSIZE;  /* FIXME: get large page size */
	uval la = lpte.bits.rpn << LOG_PGSIZE;
	uval ra = logical_to_physical_address(thr->cpu->os, la, 1ULL << lp);

	if (ra == INVALID_PHYSICAL_ADDRESS) return H_Parameter;

	uval vsid = vmc_class_vsid(thr, vmc, ea);
	if (ra == INVALID_LOGICAL_ADDRESS) return H_Parameter;

	union ptel pte;
	pte.word = 0;
	pte.bits.rpn = ra >> LOG_PGSIZE;
	pte.bits.w = lpte.bits.w;
	pte.bits.i = lpte.bits.i;
	pte.bits.m = lpte.bits.m;
	pte.bits.g = lpte.bits.g;
	pte.bits.n = ~lpte.bits.n; /* Linux pte polarity is reversed */
	pte.bits.pp0 = 0;
	if ((lpte.bits.rw & lpte.bits.dirty) ||
	    (vmc->vmc_id < NUM_KERNEL_VMC)) {
		pte.bits.pp1 = PP_RWRW >> 1;
	} else {
		pte.bits.pp1 = PP_RWRx >> 1;
	}
//	hprintf("reflect request: 0x%lx 0x%lx(0x%lx) id: %lx 0x%llx %x\n",
//		ea, linux_pte, ra, vmc->vmc_id, pte.word, pte.bits.pp1);

	sval ret = insert_ea_map(thr, vsid, ea, lpte.bits.present, pte);
	if (ret == H_Success) {
		if (lpte.bits.present) {
			hit_counter(HCNT_HPTE_INSERT);
			atomic_add32(&vmc->vmc_num_ptes,1);
		} else {
			hit_counter(HCNT_HPTE_REMOVE);
			atomic_add32(&vmc->vmc_num_ptes,(uval32)-1);
		}
	}
	return ret;
}

static sval
vmc_reflect_op(struct vm_class *vmc, struct cpu_thread* thread,
	       uval arg1, uval arg2, uval arg3, uval arg4)
{
	(void)arg4;

	switch (arg1) {
	case H_VMC_REFLECT_ENTER:
		return vmc_reflect_enter(vmc, thread, arg2, arg3);
	}
	return H_Parameter;
}


static struct vm_class_ops vmc_reflect_ops =
{
	.vmc_xlate = NULL,
	.vmc_op = vmc_reflect_op,
};

struct vm_class*
vmc_create_reflect(uval id, uval ea_base, uval size)
{
	struct vm_class *vmc = halloc(sizeof(*vmc));
	vmc_init(vmc, id, ea_base, size, &vmc_reflect_ops);
	return vmc;
}

