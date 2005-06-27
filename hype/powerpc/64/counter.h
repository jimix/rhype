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

#define hit_counter(counter_id)					\
do {								\
	sval16 cnt;						\
	asm volatile("50:\n"					\
		     "	li %0, -1\n"				\
		     "	.section __counters,\"a\";\n"		\
		     "	.align 3;\n"				\
		     "	.llong %1;\n"				\
		     "	.llong 50b;\n"				\
		     "	.previous;\n"				\
		     : "=r" (cnt) : "i" (counter_id));		\
	if (cnt >= 0 && dbg_counters)				\
		atomic_add32(&dbg_counters[cnt], 1);		\
} while (0)
#else

#define hit_counter(counter_id)

#endif /* DEBUG */


#endif /* __HYPE_PPC64_COUNTER_H__ */
