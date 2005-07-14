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

extern struct counter_fixup __start___counters[];
extern struct counter_fixup __stop___counters[];

struct counter_fixup
{
	uval64	counter_id;
	uval64	fixup_addr;
};


const struct counter_fixup* fixup_start = &__start___counters[0];
const struct counter_fixup* fixup_end = &__stop___counters[0];

uval counter_active = 0;

static inline void set_li_val(uval32 *addr, uval16 val)
{
	/* modifies the immediate value stored in the "li" instruction
	 * at "addr", so that the instruction uses "val"
	 */
	uval32 instruction = (*addr) & 0xffff0000;
	instruction |= val;
	*addr = instruction;
	hw_dcache_flush(ALIGN_DOWN((uval)addr, CACHE_LINE_SIZE));
}


static void
bind_counter(uval16 counter, uval16 slot)
{

	const struct counter_fixup *cf = fixup_start;
	while (cf < fixup_end) {
		if (counter == cf->counter_id) {
			set_li_val((uval32*)cf->fixup_addr, slot);
		}
		++cf;
	}
	dbg_counter_users[slot] = counter;
}


void
zap_and_set_counter(uval slot, uval16 type)
{
	lock_acquire(&counter_cfg_lock);

	if (dbg_counter_users[slot]) {
		bind_counter(dbg_counter_users[slot], 0xffff);
	}

	__dbg_counters[slot].hits = 0;
	__dbg_counters[slot].value = 0;

	if (type != 0) {
		bind_counter(type, slot);
	}
	lock_release(&counter_cfg_lock);
}


