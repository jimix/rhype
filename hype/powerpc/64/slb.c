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
 * Allocate and deallocate OS-related data structures.
 * This is globally locked -- if you are creating and destroying
 * OSes quickly enough to stress this lock, you have many other
 * more serious problems...
 *
 */

#include <config.h>
#include <os.h>
#include <slb.h>
#include <bitops.h>

/* switch_slb()
 *
 * The spec says that: "[an] slbmte must be preceded by an slbie (or
 * slbia) instruction that invalidates the existing translation."
 *
 * and then says: "No slbie (or slbia) is needed if the slbmte
 * instruction replaces a valid SLB entry with a mapping of a
 * different ESID (e.g., to satisfy an SLB miss).
 *
 * It may be possible that the same ESID on two partions exist for
 * different translations so better safe then sorry.
 */
void
switch_slb(struct cpu_thread *curThread, struct cpu_thread *nextThread)
{

	struct slb_cache *e = &curThread->slbcache;
	int i;

#ifdef SLB_DEBUG
	for (i = 1; i < SWSLB_SR_NUM; i++) {
		if (e[i].bits.vsid != 0) {
			hprintf("%s: LPAR[0x%x]: S%02d: 0x%016lx 0x%016lx\n",
				__func__, curThread->cpu->os->po_lpid,
				i, e[i].words.vsid, e[i].words.esid);
		}
	}
#endif

	/* invalidate all including SLB[0], we manually have to
	 * invalidate SLB[0] since slbia gets the rest */
	/* FIXME: review all of these isyncs */

	slbia();
	slbie_0();

	e = &nextThread->slbcache;

	/* restore all extra SLBs */
	uval entries = nextThread->slbcache.used_map;
	while (entries) {
		i = bit_log2(entries);
		entries &= ~(1ULL << i);

		load_slb_entry(i, e);

#ifdef SLB_DEBUG
		if (e[i].words.vsid != 0) {
			hprintf("%s: LPAR[0x%x]: R%02d: 0x%016lx 0x%016lx\n",
				__func__, nextThread->cpu->os->po_lpid,
				i, vsid, esid);
		}
#endif
	}
}

