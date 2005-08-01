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

#ifndef __PPC_HCOUNTERS__
#define __PPC_HCOUNTERS__
#include_next <hcounters.h>

/* PPC definitions go here. */

#define HCNT_SLB_MISS		0x2001 /* SLB Exceptions */
#define HCNT_KERNEL_ENTER	0x2002 /* user->kernel transition */
#define HCNT_KERNEL_EXIT	0x2003 /* kernel->user transition */
#define HCNT_PROG_EXC		0x2004 /* HV handled prog exception */

#define HCNT_CLASS_CREATE	0x2002 /* create a vm class */
#define HCNT_CLASS_DESTROY	0x2003 /* destroy a vm class */
#define HCNT_CLASS_ACTIVATE	0x2004 /* activate a vm class */
#define HCNT_CLASS_DEACTIVATE	0x2005 /* deactivate a vm class */

#define HCNT_HPTE_EVICT		0x2020 /* HPTE evictions */
#define HCNT_HPTE_MISS		0x2021 /* HPTE miss */
#define HCNT_HPTE_INSERT	0x2022 /* HPTE insertion by OS */
#define HCNT_HPTE_REMOVE	0x2023 /* HPTE removal by OS */
#define HCNT_HPTE_MODIFY	0x2024 /* HPTE modification by OS */
#define HCNT_HPTE_NOOP		0x2025

#define HCNT_SYSCALL		0x2030 /* Syscall reflection */

#define HCNT_PRIV_MFMSR		0x2040 /* mfmsr emulation */
#define HCNT_PRIV_MTMSRD	0x2041 /* mtmsrd emulation */
#define HCNT_PRIV_OTHER		0x2042 /* other SPR emulation */

#define HCNT_EXT_INT		0x2050 /* external interrupt reflection */
#define HCNT_DEC_INT		0x2051 /* decrementer interrupt reflection */
#define HCNT_MER_TRAP		0x2052 /* mediated interrupt delivery */

#define HCNT_CALL_VRFID		0x2100 /* H_VRFID invocation */
#define HCNT_CALL_MAP_SET	0x2101 /* set EA mapping call */
#define HCNT_CALL_VMC_SYNC	0x2102 /* sync active vm class call */

#define NUM_COUNTERS		16

#endif
