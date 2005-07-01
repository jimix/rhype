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
/*
 *
 * Definitions for OS-description data structures.
 *
 */

#ifndef _POWERPC_64_VM_CLASS_INLINES_H
#define _POWERPC_64_VM_CLASS_INLINES_H

#include <config.h>
#include <types.h>
#include <os.h>
#include <hcall.h>
#include <hash.h>
#include <cpu_thread.h>
#include <vm_class.h>

static inline void
vmc_put(struct vm_class *vmc)
{
	atomic_add32(&vmc->vmc_active_count, (uval32)-1);
}

static inline void
vmc_get(struct vm_class *vmc)
{
	atomic_add32(&vmc->vmc_active_count, 1);
}


static inline void
vmc_cache_slbe(struct vm_class* vmc, uval vsid)
{
	uval i = 0;
	uval idx = VMC_SLB_CACHE_SIZE;
	uval cache_val = vsid_class_offset(vsid);
	for (;i < VMC_SLB_CACHE_SIZE; ++i) {
		if (vmc->vmc_slb_cache[i] == cache_val) break;
		if (vmc->vmc_slb_cache[i] != VMC_UNUSED_SLB_ENTRY) continue;

		if (i < idx) {
			idx = i;
		}

	}
	if (idx < VMC_SLB_CACHE_SIZE) {
		vmc->vmc_slb_cache[idx] = cache_val;
	}
}

static inline uval
vmc_xlate(struct vm_class *vmc, uval eaddr, union ptel *pte)
{
	if (!vmc || !vmc->vmc_ops || !vmc->vmc_ops->vmc_xlate) {
		return INVALID_LOGICAL_ADDRESS;
	}
	if (vmc->vmc_size + vmc->vmc_base_ea - 1 < eaddr) {
		return INVALID_LOGICAL_ADDRESS;
	}
	if (vmc->vmc_base_ea > eaddr) {
		return INVALID_LOGICAL_ADDRESS;
	}

	return vmc->vmc_ops->vmc_xlate(vmc, eaddr, pte);
}


static inline uval
vmc_class_vsid(struct cpu_thread* thr, struct vm_class* vmc, uval ea)
{
	if (vmc->vmc_size + vmc->vmc_base_ea - 1 < ea) {
		return INVALID_LOGICAL_ADDRESS;
	}
	if (vmc->vmc_base_ea > ea) {
		return INVALID_LOGICAL_ADDRESS;
	}

	const uval class_space_size = ((1ULL << VM_CLASS_SPACE_SIZE_BITS));
	uval class_base = ALIGN_DOWN(vmc->vmc_base_ea, class_space_size);
	uval offset = (ea - class_base) >> LOG_SEGMENT_SIZE;

	union vm_class_vsid v;
	v.word = 0;
	v.bits.lpid_id = thr->cpu->os->po_lpid_tag;
	v.bits.class_id= vmc->vmc_id;
	v.bits.class_offset = offset;

	return v.word;
}

static inline struct vm_class*
vmc_lookup(struct cpu_thread *thr, uval id)
{
	struct vm_class *vmc = NULL;
	struct hash_entry *he = ht_lookup(&thr->cpu->os->vmc_hash, id);
	if (!he && id < NUM_KERNEL_VMC) {
		vmc = thr->cpu->os->vmc_kernel[id];
	} else if (he) {
		vmc = PARENT_OBJ(struct vm_class, vmc_hash, he);
	}

	if (vmc->vmc_flags & VMC_INVALID) {
		return NULL;
	}

	vmc_get(vmc);

	return vmc;
}

static inline void
vmc_mark_mapping_insert(struct vm_class *vmc, struct cpu_thread *thr)
{
	atomic_add32(&vmc->vmc_num_ptes,1);
#ifdef HPTE_AGING
	vmc->vmc_gen_id = htab_generation(&thr->cpu->os->htab);
#else
	(void)thr;
#endif
}

static inline void
vmc_mark_mapping_evict(struct vm_class *vmc)
{
	atomic_add32(&vmc->vmc_num_ptes,(uval32)-1);
}


#endif /* _POWERPC_64_VM_CLASS_INLINES_H */
