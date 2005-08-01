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
#ifndef __HYPE_COUNTER_H__
#define __HYPE_COUNTER_H__

#include <hcounters.h>

struct hcounter
{
	uval32 hits;
	uval value;
};

extern struct hcounter *dbg_counters;
extern struct hcounter __dbg_counters[NUM_COUNTERS];
extern uval16 dbg_counter_users[NUM_COUNTERS];

extern lock_t counter_cfg_lock;

extern void zap_and_set_counter(uval counter, uval16 type);

extern void freeze_all_counters(void);
extern void thaw_all_counters(void);
extern void counters_init(void);


#endif /* __HYPE_DEBUG_H__ */
