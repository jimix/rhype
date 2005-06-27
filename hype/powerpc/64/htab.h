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

#ifndef _HYP_POWERPC_64_HTAB_H
#define _HYP_POWERPC_64_HTAB_H

#include <config.h>
#include <types.h>
#include_next <htab.h>

//#define HPTE_AGING
#define HPTE_PURGE_AGE 3

/* hv maintains a PTE eXtension which associates other bits of data
 * with a PTE, and these are kept in a shadow array.  Currently this
 * value is the logical page number used to create the PTE.
 */
#define LOG_HPTE_AGING_LIMIT	3
struct logical_htab {
	uval sdr1;
	uval num_ptes;		/* number of PTEs in HTAB. */
	uval *shadow;		/* idx -> logical translation array */
#ifdef HPTE_AGING
	uval curr_gen;
	uval32 num_scanned;	/* number of PTEs scanned in current scan */
	uval32 num_added;	/* how many entries added since last scan */
#endif
};


#ifdef HPTE_AGING
static inline uval htab_generation(struct logical_htab *lh)
{
	return lh->curr_gen;
}

extern void htab_scan(struct logical_htab *lh);

extern uval htab_scan_range(struct logical_htab *lh, uval age,
			    uval idx, uval num);


extern uval htab_scan_pteg(struct logical_htab *lh, uval pteg);

static inline uval htab_gen_record_add(struct logical_htab* lh)
{
	uval32 val = atomic_add32(&lh->num_added, 1);
	if ((val & ((lh->num_ptes >> LOG_HPTE_AGING_LIMIT) - 1)) == 0) {
		htab_scan(lh);
	}
	return val;
}

#endif


extern void pte_insert(struct logical_htab *pt, uval ptex,
		       const uval64 vsidWord, const uval64 rpnWord, uval lrpn);



/* pte_invalidate:
 *	pt: extended hpte to use, may be NULL
 *	pte: PTE to invalidate
 */
static inline void
pte_invalidate(struct logical_htab *pt, union pte *pte)
{
	union pte *base = (union pte *)(pt->sdr1 & SDR1_HTABORG_MASK);
	uval ptex = ((uval)pte - (uval)base) / sizeof (union pte);

	pte->bits.v = 0;
	pt->shadow[ptex] = 0;
}

#endif
