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

#ifndef _CPU_THREAD_INLINES_H
#define _CPU_THREAD_INLINES_H

#include <cpu_thread.h>

#include <vregs.h>

#if !defined(HAS_MEDIATED_EE) && !defined(FORCE_APPLE_MODE)
/* Mediated interrupts not implemented */
static inline uval
thread_set_MER(struct cpu_thread *thr, uval val)
{
	(void)thr;
	(void)val;
	return 0;
}
#endif

#ifdef FORCE_APPLE_MODE


extern void __thread_set_MER(struct cpu_thread *thr);

static inline uval
test_vmode_bit(struct cpu_thread* thr, uval bit)
{
	return test_bit(thr->vstate.thread_mode, bit);
}

static inline uval is_MER_active(struct cpu_thread *thr)
{
	return (test_bit(thr->vregs->v_msr, MSR_EE) == 0)
		&& test_vmode_bit(thr, VSTATE_PENDING);
}

static inline uval
set_vmode_bit(struct cpu_thread* thr, uval bit)
{
	set_bit(thr->vstate.thread_mode, bit);
	return thr->vstate.thread_mode;
}

/* Use this for any VSTATE_PENDING_* bits */
static inline uval
set_vmode_pending(struct cpu_thread* thr, uval bit)
{
	uval old_mer = is_MER_active(thr);
	uval ret = set_vmode_bit(thr, bit);
	if (old_mer ^ is_MER_active(thr)) {
		__thread_set_MER(thr);
	}
	return ret;
}

static inline uval
clear_vmode_bit(struct cpu_thread* thr, uval bit)
{
	return clear_bit(thr->vstate.thread_mode, bit);
}



/* Use this for any VSTATE_PENDING_* bits */
static inline uval
clear_vmode_pending(struct cpu_thread* thr, uval bit)
{
	uval old_mer = is_MER_active(thr);
	uval ret = clear_vmode_bit(thr, bit);
	if (old_mer ^ is_MER_active(thr)) {
		__thread_set_MER(thr);
	}
	return ret;
}

static inline uval
thread_set_MER(struct cpu_thread *thr, uval val)
{
	if (val) {
		set_vmode_pending(thr, VSTATE_PENDING_EXT);
	} else {
		clear_vmode_pending(thr, VSTATE_PENDING_EXT);
	}
	return 0;
}
#endif /* FORCE_APPLE_MODE */
#endif /* _CPU_THREAD_INLINES_H */
