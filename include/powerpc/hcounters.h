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

#define HCNT_HPTE_EVICT		0x2001 /* HPTE evictions */
#define HCNT_SLB_MISS		0x2002 /* SLB Exceptions */
#define HCNT_CLASS_CREATE	0x2003 /* create a vm class */
#define HCNT_CLASS_DESTROY	0x2004 /* destroy a vm class */
#define HCNT_CLASS_ACTIVATE	0x2005 /* activate a vm class */
#define HCNT_CLASS_DEACTIVATE	0x2006 /* deactivate a vm class */
#define HCNT_HPTE_MISS		0x2007 /* HPTE miss */
#define HCNT_HPTE_INSERT	0x2008 /* HPTE insertion by OS */
#define HCNT_HPTE_REMOVE	0x2009 /* HPTE removal by OS */
#define HCNT_SYSCALL		0x200a /* Syscall reflection */
#define HCNT_PRIV_MFMSR		0x200b
#define HCNT_PRIV_MTMSRD	0x200c
#define HCNT_PRIV_OTHER		0x200d

#define HCNT_EXT_INT		0x200e
#define HCNT_MER_TRAP		0x200f

#define HCNT_CALL_VRFID		0x2100


#define NUM_COUNTERS		16

#endif
