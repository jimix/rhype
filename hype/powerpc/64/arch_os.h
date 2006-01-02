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

#ifndef _POWERPC_64_OS_H
#define _POWERPC_64_OS_H

#ifndef _HYP_GENERIC_OS_H
#error	"arch_os.h must not be included directly. Include os.h instead."
#endif

#ifndef __ASSEMBLY__

#include <lpar.h>
#include <xir.h>
#include <ipc.h>
#include <io_chan.h>
#include <logical.h>
#include <vtty.h>
#include <partition.h>
#include <aipc.h>
#ifdef FORCE_APPLE_MODE
#include <vm_class.h>
#endif

/*
 * Hypervisor-call vector.  Provide for multiple vectors to allow
 * (for example) a single hypervisor to support both 32-bit and 64-bit
 * OSes.  Put in struct to allow for the possibility of adding additional
 * fields.
 */

typedef sval (*hcall_vec_t) ( /* struct cpu_thread *thread, ... */ );

/* Scheduling parameters.  Describes the scheduling policy for this OS. */
/* 64-bit bit-map for 64 scheduling slots */
#define NUM_SCHED_SLOTS ((uval)(sizeof (uval64) * 8))
#define SLOT_CYCLES	0x50000

/* values for pco_preempt field */
#define PREEMPT_RESCHEDULE		0x1	/* go through schedulers */
#define PREEMPT_SELF			0x2	/* scheduling not needed; deliver int */
#define PREEMPT_RESCHEDULE_DIRECTED	0x4	/* go through schedulers for a specific os */

/*
 * Per-OS data structure.  This holds OS state (running, sleeping, etc.)
 * and priority information.
 *
 * Unless otherwise noted, all fields are guarded by po_mutex.
 */

#ifndef HAS_RMOR
#error ppc64 expects CPUs have RMOR
#endif
struct os {
	uval64 rmo;
	uval64 rmo_size;
	/* pointers and uvals first */
	struct cpu *cpu[MAX_CPU];
	/* in case MAX_CPU is odd */
	char _pad_cpu[(sizeof (char *) % 8) * (MAX_CPU % 2)];
	/* 64 bit aligned here */
#ifdef HAS_SPC_OS_DATA
	struct spc_os_data po_spc_data[BP_SPUS];
	uval os_raid;
#endif

	/* 64 bit aligned here */
	struct hash_entry os_hashentry;
	struct partition_info *os_partition_info;
	struct partition_info cached_partition_info[2];
	struct logical_memory_map logical_mmap;
	struct aipc_services aipc_srv;
	struct hash_table po_xirr_map;
	struct io_chan *po_chan[CHANNELS_PER_OS];
	uval po_vt[2];
	struct page_share_mgr *po_psm;
#ifdef FORCE_APPLE_MODE
	struct vm_class *vmc_kernel[NUM_KERNEL_VMC];
	struct hash_table vmc_hash;
#endif
	uval po_boot_msr;	/* MSR for booting OS. */
	uval po_exception_msr;	/* MSR for exceptions to OS. */

	struct os_events po_events;

	struct logical_htab htab;

	/* Resources we've been granted access to */
	struct dlist po_resources;

	/* 64 bit aligned here */
	/* 32 bit members */
	rw_lock_t po_mutex;	/* Per-OS lock. */
	uval32 po_state;	/* OS state. */
	uval32 po_lpid;		/* OS ID (16 bits on 32-bit impls). */
	uval32 use_large_pages;

	/* 64 bit aligned here */
	/* 16 bit members */
	uval16 po_lpid_tag;	/* OS index, not an ID. */
	uval16 installed_cpus;
	uval16 large_page_selection;
	uval16 large_page_shift[2];
	uval16 _pad[3];
};



static inline uval
hv_features(struct os* os)
{
#ifdef FORCE_APPLE_MODE
	return os != os;
#else
	return os->rmo != INVALID_PHYSICAL_ADDRESS;
#endif
}

extern struct os *ctrl_os;

extern void switch_tlb(struct cpu_thread *new_thread);

/* Returns 0 if nothing changes ,
 * Returns 1 if exception to current thread,
 * Returns 2 if thread switch required.
 */
extern uval handle_external(struct cpu_thread *thread);

#ifdef FORCE_APPLE_MODE
static inline uval
return_arg(struct cpu_thread *thread, uval argnum, uval arg)
{
	if (argnum == 0 || argnum > 5)
		return 1;

	thread->return_args->reg_gprs[argnum + 3] = arg;
	return 0;
}
#else
static inline uval
return_arg(struct cpu_thread *thread, uval argnum, uval arg)
{
	if (argnum == 0 || argnum > 5)
		return 1;

	thread->reg_gprs[argnum + 3] = arg;
	return 0;
}
#endif

static inline uval
hv_map_LA(struct os *os, uval laddr, uval size)
{
	return logical_to_physical_address(os, laddr, size);
}

/* Set current thread state to exception vector.
 * Modifies "thread" only, does not set register.
 */
extern void set_to_exception(struct cpu_thread *thread, uval vector,
			     uval curr_pc, uval curr_msr);

extern void switch_large_page_support(struct cpu_thread *cur,
				      struct cpu_thread *next);
extern void restore_large_page_selection(struct cpu_thread *);
extern void hype_init(uval r3, uval r4, uval r5, uval r6, uval r7,
		      uval orig_msg);
#endif /* ! __ASSEMBLY__ */

/* values for the po_state field. */

#define PO_STATE_EMPTY		0	/* Empty slot. */
#define PO_STATE_CREATED	1	/* Created, but not yet started. */
#define PO_STATE_RUNNABLE	2	/* Runnable. */
#define PO_STATE_BLOCKED	2	/* Blocked, e.g., wrong timeslot. */
#define PO_STATE_TERMINATED	4	/* Terminated, but status not */
					/*  collected. */
#endif /* ! _POWERPC_64_OS_H */
