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

extern unsigned long start__counters[0];
extern unsigned long end__counters[0];

struct counter_fixup
{
	uval64	counter_id;
	uval64	fixup_addr;
};


struct counter_fixup* fixup_start = (struct counter_fixup*)&start__counters[0];
struct counter_fixup* fixup_end = (struct counter_fixup*)&end__counters[0];

static uval32 *counter_ins_addr[NUM_COUNTERS] = { NULL,};
uval counter_active = 0;

static inline void set_li_val(uval32 *addr, uval16 val)
{
	uval32 instruction = (*addr) & 0xffff0000;
	instruction |= val;
	*addr = instruction;
	hw_dcache_flush(ALIGN_DOWN((uval)addr, CACHE_LINE_SIZE));
}


void
zap_and_set_counter(uval counter, uval16 type)
{
	lock_acquire(&counter_cfg_lock);

	if (counter_ins_addr[counter]) {
		set_li_val(counter_ins_addr[counter], 0xffff);
	}

	struct counter_fixup *cf = fixup_start;
	while (cf < fixup_end) {
		if (type == cf->counter_id) {
			break;

		}
		++cf;
	}

	dbg_counters[counter] = 0;
	if (cf == fixup_end) goto done;


	/* modifies the immediate value stored in the "li" instruction
	 * at "addr", so that the instruction uses "type"
	 */
	set_li_val((uval32*)cf->fixup_addr, counter);

	counter_ins_addr[counter] = (uval32*)cf->fixup_addr;
	dbg_counter_users[counter] = type;
done:
	lock_release(&counter_cfg_lock);
}


