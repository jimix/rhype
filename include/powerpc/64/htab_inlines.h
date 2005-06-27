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

#ifndef _POWERPC_64_HTAB_INLINES_H
#define _POWERPC_64_HTAB_INLINES_H

#include <config.h>
#include <types.h>
#include <atomic.h>

#include <mmu.h>
#include <mmu_defs.h>
#include <bitmap.h>


#ifdef HPTE_AGING
static inline void pte_set_gen(union pte* pte, uval val)
{
	pte->bits.res = val & 1;
	pte->bits.sw = (val >> 1) & 1;
}

static inline uval pte_gen_value(union pte* pte)
{
	return (pte->bits.sw << 1) + pte->bits.res;
}

static inline uval pte_immortal(union pte* pte)
{
	return pte->bits.bolted;
}

static inline uval pte_age(union pte* pte, uval htab_age)
{
	uval v = pte_gen_value(pte);
	uval base = htab_age & 0x3;
	if (base + v > htab_age) {
		base -= 4;
	}
	return base + v;
}

#endif

static inline uval
pte_islocked(union pte *pte)
{
	return pte->bits.lock;
}

static inline uval
pte_trylock(union pte *pte)
{
	union pte locked;
	union pte old;
	old = locked = *pte;
	locked.bits.lock = 1;
	if (old.bits.lock == 0 &&
	    cas_uval64(&pte->words.vsidWord, old.words.vsidWord,
		       locked.words.vsidWord)) {
		sync_after_acquire();
		return 1;
	}
	return 0;
}

static inline void
pte_lock(union pte *pte)
{
	while (!pte_trylock(pte));
}

static inline void
pte_unlock(union pte *pte)
{
	sync_before_release();
	pte->bits.lock = 0;
}

static inline uval64
vpn_from_pte(union pte *pte, uval pteg)
{
	if (pte->bits.h) pteg = ~pteg;
	uval vsid = pte->bits.avpn >> 5;
	uval p = (vsid ^ pteg) & 0x7ff;
	p |= (pte->bits.avpn & 0x1f) << 11;
	return (vsid << (LOG_SEGMENT_SIZE - LOG_PGSIZE)) | p;
}

static inline uval64
vpn_from_vsid_ea(uval64 vsid, uval64 ea)
{
	return ((ea & (SEGMENT_SIZE-1)) >> LOG_PGSIZE) |
		(vsid << (LOG_SEGMENT_SIZE - LOG_PGSIZE));

}


/* May perform an unlock operation if new_pte doesn't have lock bit set */
static inline void
htab_entry_modify(union pte *pte, uval64 vpn, union pte *new_pte)
{
	uval large = pte->bits.l;
	if (pte->bits.v) {
		uval va = vpn << LOG_PGSIZE;

		clear_bit(va, 0xffffULL << 48);

		pte->bits.v = 0;
		ptesync();
		if (large) {
			/* FIXME: should erase low-order bits, but for that
			 * we need to know the page size
			 */
			tlbie_large(vpn);
		} else {
			clear_bit(va, PGSIZE - 1);
			tlbie(va);
		}
		eieio();
		tlbsync();
		ptesync();
	}

	if (new_pte && new_pte->bits.v) {
		pte->words.rpnWord = new_pte->words.rpnWord;
		eieio();
		pte->words.vsidWord = new_pte->words.vsidWord;
		ptesync();
	}
}

static inline void
htab_entry_delete(union pte *pte, uval64 vpn)
{
	htab_entry_modify(pte, vpn, NULL);
}



#endif
