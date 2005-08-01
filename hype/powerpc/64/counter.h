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
 * Useful debug routines.
 *
 */
#ifndef __HYPE_PPC64_COUNTER_H__
#define __HYPE_PPC64_COUNTER_H__

#include <config.h>
#include <types.h>
#include <atomic.h>
#include_next <counter.h>


#ifdef DEBUG

#define hit_counter(counter_id)	hit_counter_cond(counter_id,1)


#define counter_id(name)					\
({								\
	sval16 cnt = -1;					\
	asm volatile("50:\n"					\
		     "	li %0, -1\n"				\
		     "	.section __counters,\"a\";\n"		\
		     "	.align 3;\n"				\
		     "	.llong %1;\n"				\
		     "	.llong 50b;\n"				\
		     "	.previous;\n"				\
		     : "=r" (cnt) : "i" (name));		\
	cnt;							\
})								\



#define hit_counter_cond_action(name, cond, action)		\
do {								\
	sval16 cnt = counter_id(name);				\
	if (likely(cnt < 0)) break;				\
	if (unlikely(dbg_counters == NULL)) break;		\
	if (!(cond)) break;					\
	atomic_add32(&dbg_counters[cnt].hits, 1);		\
	action;							\
} while (0)

#define hit_counter_cond(name, cond) hit_counter_cond_action(name, cond,)

#define add_counter_val_cond(name, val, cond)			\
do {								\
	sval16 cnt = counter_id(name);				\
	if (likely(cnt < 0)) break;				\
	if (unlikely(dbg_counters == NULL)) break;		\
	if (!(cond)) break;					\
	atomic_add(&dbg_counters[cnt].value, val);		\
} while (0);

#define start_timing_counter(name)				\
	uval __tmp_##name = 0;					\
	hit_counter_cond_action(name, 1, __tmp_##name = mftb());


#define end_timing_counter(name)				\
	add_counter_val_cond(name, mftb() - __tmp_##name, 1);


#else

#define hit_counter(counter_id)
#define hit_counter_cond(name, cond)
#define start_timing_counter(name)
#define end_timing_counter(name)
#define hit_counter_cond_action(name, cond, action)

#endif /* DEBUG */


#endif /* __HYPE_PPC64_COUNTER_H__ */
