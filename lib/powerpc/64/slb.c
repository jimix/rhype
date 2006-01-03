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
#include <bitops.h>

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
	   struct slb_cache *slbc)
{
	int i;
	int spot = SWSLB_SR_NUM;
	union slb_entry tmp;

	slb_calc(ea, is_lp, lp_selection, kp, vsid, &tmp);
	uval entries = slbc->used_map;
	while (entries) {
		i = bit_log2(entries);
		entries &= ~(1ULL << i);

		union slb_entry *curr;
		curr = &slbc->entries[i];

		if (tmp.bits.esid == curr->bits.esid) {
			spot = i;
		}

	}

	if (spot == SWSLB_SR_NUM) {
		spot = bit_log2(~slbc->used_map);
	}


	if (spot < SWSLB_SR_NUM) {
		tmp.bits.index = spot;
		if (tmp.words.esid == slbc->entries[spot].words.esid &&
		    tmp.words.vsid == slbc->entries[spot].words.vsid) {
			return spot;
		}

		set_slb_entry(spot, slbc, &tmp);
		return spot;
	}

	assert(0, "slb_insert: we don't invalidate slbe's yet\n");
	for (;;);
	/* when you do invalidate and replace make sure you leave
	 * SLB[0] alone */
}

int
slb_insert_to_slot(uval spot, uval ea, uval8 is_lp, uval8 lp_selection,
		   uval kp, uval vsid, struct slb_cache *slbc)
{
	union slb_entry tmp;

	slb_calc(ea, is_lp, lp_selection, kp, vsid, &tmp);

	assert(slbc, "No slbe cache given\n");

	set_slb_entry(spot, slbc, &tmp);

	return spot;
}
void
assert_slb_cache(struct slb_cache *slbc)
{
	int i;
	int j;
	for (i = 0; i < SWSLB_SR_NUM; j++) {
		union slb_entry *curr = &slbc->entries[i];
		union slb_entry hw;
		__get_slb_entry(i, &hw);
		assert(hw.bits.v == curr->bits.v , "Valid entry mismatch\n");

		for (j = i + 1; j < SWSLB_SR_NUM; j++) {
			assert(slbc->entries[j].bits.esid != curr->bits.esid,
			       "Duplicate ESID in slb cache\n");

		}

	}

}

static inline void
print_slbe(const char *prefix, int index, union slb_entry *curr)
{
	hprintf("%s %2d: %013llx %c %c %c %c %c %09llx %c %03x\n",
		prefix, index, curr->bits.vsid,
		(curr->bits.ks? 'S': ' '),
		(curr->bits.kp? 'P': ' '),
		(curr->bits.n ? 'N': ' '),
		(curr->bits.l ? 'L': ' '),
		(curr->bits.c ? 'C': ' '),
		curr->bits.esid,
		(curr->bits.v ? 'V': ' '),
		curr->bits.index);
}



void
slb_dump(struct slb_cache *slbc)
{
	int i;
	for (i = 0; i < SWSLB_SR_NUM; i++) {
		union slb_entry *curr = &slbc->entries[i];
		union slb_entry hw;
		__get_slb_entry(i, &hw);
		if (curr->words.vsid || curr->words.esid) {
			print_slbe("C", i, curr);
		}
		hw.bits.index = curr->bits.index;
		if (hw.words.vsid != curr->words.vsid ||
		    hw.words.esid != curr->words.esid) {
			print_slbe("H", i, &hw);
		}
	}

}


#ifdef FORCE_APPLE_MODE
void
save_slb(struct slb_cache *cache)
{
	/* In apple mode we have it cached/saved already */
	(void)cache;
}

#else /* FORCE_APPLE_MODE */
void
save_slb(struct slb_cache *slbc)
{
	int i;

	/* save all extra SLBs */
	for (i = 0; i < SWSLB_SR_NUM; i++) {

		get_slb_entry(i, slbc);
#ifdef SLB_DEBUG
		if (slbc->entry[i].words.vsid != 0) {
			hprintf("%s: %p: S%02d: 0x%016lx 0x%016lx\n",
				__func__, slbc, i, vsid, esid);
		}
#endif
	}
	slbc->used_map = ~0ULL;
}
#endif /* FORCE_APPLE_MODE */


void
restore_slb(struct slb_cache *slbc)
{
	int i;

	/* invalidate all including SLB[0], we manually have to
	 * invalidate SLB[0] since slbia gets the others only */
	/* FIXME: review all of these isyncs */

	slbia();
	slbie_0();

	/* restore all extra SLBs */
	uval64 entries = slbc->used_map;
	while (entries) {
		i = bit_log2(entries);
		entries &= ~(1ULL << i);

		load_slb_entry(i, slbc);

#ifdef SLB_DEBUG
		if (slb_entry[i].words.vsid != 0) {
			hprintf("%s: %p: R%02d: 0x%016lx 0x%016lx\n",
				__func__, slbc, i, vsid, esid);
		}
#endif
	}
}



