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
 * slb routines
 *
 */

#include <config.h>
#include <types.h>
#include <lib.h>
#include <mmu.h>


/* does everything but set the index in the vsid */
static void
slb_calc(uval ea, uval8 is_lp, uval8 lp_selection, uval kp, uval vsid,
	 union slb_entry *se)
{
	se->words.vsid = 0;
	se->words.esid = 0;

	se->bits.vsid = vsid;
	se->bits.esid = ea >> LOG_SEGMENT_SIZE;

	se->bits.v = 1;

	if (is_lp) {
#ifdef DEBUG
		hprintf("slb_insert for large page - selection = %d\n",
			lp_selection);
#endif
		se->bits.l = 1;  /* large page   */
		se->bits._zeroes1 = lp_selection; /* lp selection */
	}

	se->bits.kp = kp; /* Kp = <kp> */
}


int
slb_insert(uval ea, uval8 is_lp, uval8 lp_selection, uval kp, uval vsid,
	   union slb_entry *slbe_cache)
{
	int i;
	int spot = 64;
	union slb_entry tmp;

	slb_calc(ea, is_lp, lp_selection, kp, vsid, &tmp);
	for (i = 0; i < SWSLB_SR_NUM; i++) {
		/* The first one (SLB[0] always belongs to the kernel
		 * and slbia will not invalidate that one
		 */
		union slb_entry __tmp;
		union slb_entry *curr;
		/* If cache is given, look there */
		if (slbe_cache) {
			curr = &slbe_cache[i];
		} else {
			get_slb_entry(i, &__tmp);
			curr = &tmp;
		}


		/* check if not valid */
		/* choose lowest empty spot */
		if (curr->bits.v == 0 && spot > i) {
			spot = i;
		}
		if (tmp.bits.esid == curr->bits.esid) {
			spot = i;
			break;
		}

	}

	if (spot < SWSLB_SR_NUM) {
		tmp.bits.index = spot;
		if (tmp.words.esid == slbe_cache[spot].words.esid &&
		    tmp.words.vsid == slbe_cache[spot].words.vsid) {
			return spot;
		}

		set_slb_entry(spot, slbe_cache, &tmp);
		return spot;
	}

	assert(0, "slb_insert: we don't invalidate slbe's yet\n");
	for (;;);
	/* when you do invalidate and replace make sure you leave
	 * SLB[0] alone */
}

int
slb_insert_to_slot(uval spot, uval ea, uval8 is_lp, uval8 lp_selection,
		   uval kp, uval vsid, union slb_entry *slbe_cache)
{
	union slb_entry tmp;

	slb_calc(ea, is_lp, lp_selection, kp, vsid, &tmp);

	assert(slbe_cache, "No slbe cache given\n");

	set_slb_entry(spot, slbe_cache, &tmp);

	return spot;
}
