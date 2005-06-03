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

#include <types.h>
#include <lib.h>
#include <test.h>
#include <mmu.h>
#include <objalloc.h>
#include <hcall.h>
#include <test.h>

struct mmio_range *ranges = NULL;

void
save_mmio_range(uval base, uval size, const char* name)
{
	struct mmio_range *mr = halloc(sizeof(struct mmio_range));
	mr->mr_base = base;
	mr->mr_size = size;
	mr->mr_name = name;
	dlist_init(&mr->mr_list);

	if (!ranges) {
		ranges = mr;
		return;
	}


	struct mmio_range *cur = ranges;

	do {
		if (range_subset(cur->mr_base, cur->mr_size,
				 mr->mr_base, mr->mr_size)) {
			return;
		}

		if (cur->mr_base > mr->mr_base) {
			dlist_ins_before(&mr->mr_list, &cur->mr_list);
			if (cur == ranges) ranges = mr;
			return;
		}

		cur = PARENT_OBJ(struct mmio_range, mr_list,
				 dlist_next(&cur->mr_list));
	} while (cur != ranges);

	dlist_ins_before(&mr->mr_list, &ranges->mr_list);

}


void
define_all_mmio_ranges(void)
{
	struct mmio_range *cur = ranges;
	struct mmio_range *next;

	uval start;
	uval stop;
	
	start = cur->mr_base;
	stop = start + cur->mr_size;
	do {

		next = PARENT_OBJ(struct mmio_range, mr_list,
				  dlist_next(&cur->mr_list));


		if (next != cur &&
		    next->mr_base < ALIGN_UP(stop, 1ULL <<LOG_CHUNKSIZE)) {
			stop = next->mr_base + next->mr_size;
			dlist_detach(&next->mr_list);
			hfree(next, sizeof(*next));
			continue;
		}


		sval rc;
		uval rets[1];

		rc = hcall_mem_define(rets, MMIO_ADDR, start, stop - start);
		hprintf("Defined LA MMIO range: 0x%lx[0x%lx] -> 0x%lx\n",
			start, stop - start, rets[0]);

		dlist_detach(&cur->mr_list);
		hfree(cur, sizeof(*cur));

		if (next == cur) break;

		ranges = next;
		cur = next;

		start = cur->mr_base;
		stop = start + cur->mr_size;

	} while (1);
}
