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

#ifndef _POWERPC_64_VM_CLASS_H
#define _POWERPC_64_VM_CLASS_H

#include <config.h>
#include <types.h>
#include <os.h>
#include <hcall.h>
#include <hash.h>
#include <cpu_thread.h>

#define VM_CLASS_LPID_BITS	4
#ifdef LINUX_2_6_13
#define VM_CLASS_ID_BITS	17
#define VM_CLASS_BITS		16
#else
#define VM_CLASS_ID_BITS	20
#define VM_CLASS_BITS		13
#endif

#define VM_CLASS_SIZE	(1ULL << (VM_CLASS_BITS + LOG_SEGMENT_SIZE))
union vm_class_vsid
{
	struct
	{
		uval64	_unused:27;
		uval64	class_offset:VM_CLASS_BITS;
		uval64	class_id:VM_CLASS_ID_BITS;
		uval64	lpid_id:VM_CLASS_LPID_BITS;
	}bits;
	uval64 word;
};

static inline uval vsid_class_id(uval64 vsid)
{
	union vm_class_vsid x = { .word = vsid };
	return x.bits.class_id;
}

static inline uval vsid_lpid_id(uval64 vsid)
{
	union vm_class_vsid x = { .word = vsid };
	return x.bits.lpid_id;
}

static inline uval vsid_class_offset(uval64 vsid)
{
	union vm_class_vsid x = { .word = vsid };
	return x.bits.class_offset;
}


union linux_pte
{
	uval64 word;
	struct
	{
		uval64	rpn	:47;
		uval64	huge	:1;
		uval64	grp_idx	:3;
		uval64	scnd	:1;
		uval64	busy	:1;
		uval64	hashpte	:1;
		uval64	rw	:1;
		uval64	accessed:1;
		uval64	dirty	:1;
		uval64	w	:1;
		uval64	i	:1;
		uval64	m	:1;
		uval64	g	:1;
		uval64	n	:1;
		uval64	user	:1;
		uval64	present	:1;
	} bits;
};

struct hvpr_table_map
{
	uval64		base_addr;
	union linux_pte	base_pte;
	uval64		num_pages;
};

struct vm_class;

struct vm_class_ops
{
	/* If logical address is invalid, and pte is non-zero, then
	 * pte contains physical address. Pte is initially 0. */
	uval (*vmc_xlate)(struct vm_class *vmc, uval eaddr, union ptel *pte);

	/* If a VM class has this method, it becomes totally responsible
	 * for processing the rest of the page-table miss.
	 */
	uval (*vmc_exception)(struct vm_class *vmc, struct cpu_thread *thr,
			      struct vexc_save_regs *vr, uval eaddr);

	sval (*vmc_op)(struct vm_class *vmc, struct cpu_thread *thr,
		       uval arg1, uval arg2, uval arg3, uval arg4);
	void (*vmc_dealloc)(struct vm_class *vmc);
};

#define VMC_SLB_CACHE_SIZE	8

#define VMC_UNUSED_SLB_ENTRY	0xffff

#define VMC_INVALID	0x1
#define VMC_DEBUG	0x2
struct vm_class {
	struct hash_entry vmc_hash;
#define vmc_id vmc_hash.he_key
	uval vmc_flags;
	uval vmc_base_ea; /* effective base address */
	uval vmc_size;
	uval32 vmc_active_count;
#ifdef HPTE_AGING
	uval64 vmc_gen_id;	/* generation id of last insertion */
#endif
	uval32 vmc_num_ptes; /* number of pte's inserted by class */
	uval vmc_slb_entries;
	uval16 vmc_slb_cache[VMC_SLB_CACHE_SIZE];
	uval vmc_data;	/* implementation specific data */
	struct vm_class_ops *vmc_ops;
};


extern uval vmc_init(struct vm_class *vmc, uval id, uval base, uval size,
		     struct vm_class_ops *ops);

extern struct vm_class* vmc_create_vregs(struct cpu_thread *thr);

extern struct vm_class* vmc_create_linear(uval id, uval base, uval size,
					  uval offset);
extern struct vm_class* vmc_create_table(struct cpu_thread* thr, uval id,
					 uval ea_base, uval tbl_base,
					 uval size);

extern struct vm_class* vmc_create_reflect(uval id, uval ea_base, uval size);

extern struct vm_class* find_kernel_vmc(struct cpu_thread* thr, uval eaddr);
extern struct vm_class* find_app_vmc(struct cpu_thread* thr, uval eaddr);

extern sval vmc_destroy(struct cpu_thread *thread, struct vm_class *vmc);


/* Temporarily disable all mappings associate with this vmc */
extern void vmc_deactivate(struct cpu_thread *thread, struct vm_class *vmc);
/* Enable mappings associated with this vmc */
extern void vmc_activate(struct cpu_thread *thread, struct vm_class *vmc);

#define PTE_VALID	1
#define PTE_LOCKED	2
#define PTE_BOLTED	4
extern sval vmc_set_ea_map(struct cpu_thread *thr, struct vm_class* vmc,
			   uval ea, uval flags, union ptel pte);

#define LOCATE_MATCH	1	/* return failure if no pte for this vpn */
#define LOCATE_NO_EVICT	2	/* no eviction if no match found */
extern union pte* locate_clear_lock_target(struct cpu_thread* thr, uval vpn,
					   uval flags);

extern uval htab_purge_vmc(struct cpu_thread *thr, struct vm_class* vmc);


#endif /* _POWERPC_64_VM_CLASS_H */
