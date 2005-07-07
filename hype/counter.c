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
#include <hype.h>
#include <counter.h>

struct hcounter *dbg_counters = NULL;
struct hcounter __dbg_counters[NUM_COUNTERS];
uval16 dbg_counter_users[NUM_COUNTERS] = { 0, };

lock_t counter_cfg_lock = lock_unlocked;

void
freeze_all_counters(void)
{
	lock_acquire(&counter_cfg_lock);

	dbg_counters = NULL;
	sync_after_acquire();

	lock_release(&counter_cfg_lock);
}

void
thaw_all_counters(void)
{
	lock_acquire(&counter_cfg_lock);

	sync_before_release();
	dbg_counters = __dbg_counters;

	lock_release(&counter_cfg_lock);
}
