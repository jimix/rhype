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
#include <asm.h>
#include <lpar.h>
#include <lib.h>
#include <htab.h>
#include <h_proto.h>
#include <hype.h>
#include <os.h>
#include <mmu.h>
#include <pmm.h>
#include <objalloc.h>
#include <bitops.h>
#include <debug.h>
#include <htab_inlines.h>

static uval
htab_calc_sdr1(uval htab_addr, uval log_htab_size)
{
	uval sdr1_htabsize;
	uval htab_size = 1UL << log_htab_size;

	assert((htab_addr & (htab_size - 1)) == 0,
	       "misaligned htab address (0x%lx)\n", htab_addr);

	assert(log_htab_size <= SDR1_HTABSIZE_MAX, "htab too big (0x%lx)",
			htab_size);

	assert(log_htab_size >= HTAB_MIN_LOG_SIZE, "htab too small (0x%lx)",
			htab_size);

	sdr1_htabsize = log_htab_size - LOG_PTEG_SIZE - SDR1_HTABSIZE_BASEBITS;

	return (htab_addr | (sdr1_htabsize & SDR1_HTABSIZE_MASK));
}

/* pte_insert: called by h_enter
 *	pt: extended hpte to use
 * 	ptex: index into page-table to use
 * 	vsidWord: word containing the vsid
 * 	rpnWord: word containing the rpn
 *	lrpn: logial address corresponding to pte
 */
void
pte_insert(struct logical_htab *pt, uval ptex, const uval64 vsidWord,
	   const uval64 rpnWord, uval lrpn)
{
	union pte *chosen =
		((union pte *)(pt->sdr1 & SDR1_HTABORG_MASK)) + ptex;
	uval *shadow = pt->shadow + ptex;

	/*
	 * It's required that external locking be done to
	 * provide exclusion between the choices of insertion
	 * points.
	 * Any valid choice of pte requires that the pte be
	 * invalid upon entry to this function.
	 * XXX lock etc
	 * LPAR 18.5.4.1.2 "Algorithm" requires ldarx/stdcx etc.
	 * and using bit 57 for per-pte locking.
	 */
	assert((chosen->bits.v == 0), "Attempt to insert over valid PTE\n");

	/* Set the second word first so the valid bit is the last thing set */
	chosen->words.rpnWord = rpnWord;

	/* Set shadow word. */
	*shadow = lrpn;

	/* Guarantee the second word is visible before the valid bit */
	eieio();

	/* Now set the first word including the valid bit */
	chosen->words.vsidWord = vsidWord;
	ptesync();
}

/* htab_alloc()
 * - reserve an HTAB for this cpu
 * - set cpu's SDR1 register
 */
void
htab_alloc(struct os *os, uval log_htab_bytes)
{
	uval sdr1_val = 0;
	uval htab_raddr;
	uval htab_bytes = 1UL << log_htab_bytes;
	int i;

#ifdef FORCE_APPLE_MODE
	if (os != ctrl_os) {
		os->htab.num_ptes = ctrl_os->htab.num_ptes;
		os->htab.sdr1 = ctrl_os->htab.sdr1;
		os->htab.shadow = ctrl_os->htab.shadow;

		for (i = 0; i < os->installed_cpus; i++) {
			os->cpu[i]->reg_sdr1 = os->htab.sdr1;
		}
		return;
	}
#endif

	htab_raddr = get_pages_aligned(&phys_pa, htab_bytes, log_htab_bytes);
	assert(htab_raddr != INVALID_PHYSICAL_PAGE, "htab allocation failure\n");

	/* hack.. should make this delta global */
	htab_raddr += get_hrmor();

	/* XXX slow. move memset out to service partition? */
	memset((void *)htab_raddr, 0, htab_bytes);

	sdr1_val = htab_calc_sdr1(htab_raddr, log_htab_bytes);

	os->htab.num_ptes = (1ULL << (log_htab_bytes - LOG_PTE_SIZE));
	os->htab.sdr1 = sdr1_val;
	os->htab.shadow = (uval *)halloc(os->htab.num_ptes * sizeof (uval));
	assert(os->htab.shadow, "Can't allocate shadow table\n");

	for (i = 0; i < os->installed_cpus; i++) {
		os->cpu[i]->reg_sdr1 = sdr1_val;
	}

	DEBUG_OUT(DBG_MEMMGR, "LPAR[0x%x] hash table: RA 0x%lx; 0x%lx entries\n",
		os->po_lpid, htab_raddr, os->htab.num_ptes);
}

uval
htab_purge_vmc(struct cpu_thread *thr, struct vm_class* vmc)
{
	uval num_purged = 0;

	uval end = thr->cpu->os->htab.num_ptes;
	uval i = 0;
	union pte *ht = (union pte*)GET_HTAB(thr->cpu->os);
	for (; i < end; ++i) {
		/* vsid is 52 bits, avpn has 57 */
		uval pte_vsid = ht[i].bits.avpn >> 5;

		if (ht[i].bits.v == 0) continue;
		if (vsid_class_id(pte_vsid) != vmc->vmc_id) continue;
		if (vsid_lpid_id(pte_vsid) != thr->cpu->os->po_lpid_tag)
			continue;

		pte_lock(&ht[i]);

		uval vpn = vpn_from_pte(&ht[i], i >> LOG_NUM_PTES_IN_PTEG);
		htab_entry_modify(&ht[i], vpn, NULL);
		pte_unlock(&ht[i]);
		++num_purged;
	}
	return num_purged;
}


void
htab_free(struct os *os)
{
	uval ht_ra = os->htab.sdr1 & SDR1_HTABORG_MASK;
	uval ht_ea;

	/* hack.. should make the hv_ra delta global */
	ht_ea = ht_ra - get_hrmor();
	if (ht_ea) {
		free_pages(&phys_pa, ht_ea,
			   os->htab.num_ptes << LOG_PTE_SIZE);
	}

	if (os->htab.shadow)
		hfree(os->htab.shadow, os->htab.num_ptes * sizeof (uval));

	DEBUG_OUT(DBG_MEMMGR, "HTAB: freed htab at 0x%lx\n", ht_ea);
}

#ifdef HPTE_AGING
uval
htab_scan_range(struct logical_htab *lh, uval age, uval idx, uval num)
{
	union pte *htab = (union pte*)(lh->sdr1 & SDR1_HTABORG_MASK);
	union pte *p = htab + idx;
	uval i = 0;
	uval curr = htab_generation(lh);
	uval num_purged = 0;

	for (; i < num; ++i, ++p) {

		if (pte_islocked(p)) continue;
		if (pte_immortal(p)) continue;
		if (curr - pte_age(p, curr) < age) continue;
		if (!pte_trylock(p)) continue;

		uval pteg = ((uval)((p - htab))) >> LOG_NUM_PTES_IN_PTEG;
		uval vpn = vpn_from_pte(p, pteg);
		htab_entry_delete(p, vpn);
		pte_unlock(p);
		++num_purged;
	}
	return num_purged;
}

void
htab_scan(struct logical_htab *lh)
{
	/* FIXME need a lock here, for modifications to lh struct */
	uval scan_size = lh->num_ptes >> LOG_HPTE_AGING_LIMIT;
	uval end_scan = lh->num_scanned + scan_size;
	uval idx = lh->num_scanned;

	htab_scan_range(lh, idx, HPTE_PURGE_AGE, scan_size);

	if (end_scan == lh->num_ptes) {
		++lh->curr_gen;
		lh->num_scanned = 0;
	} else {
		lh->num_scanned = end_scan;
	}

}
#endif
