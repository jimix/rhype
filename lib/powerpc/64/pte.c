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
#include <mmu.h>
#include <hcall.h>
#include <lpar.h>
#include <lib.h>
#include <htab.h>
uval
get_esid(uval ea)
{
	return (ea & (~0ULL << (63 - 35)));
}

uval
get_vsid(uval ea)
{
	uval vsid_val;
	uval esid_val = get_esid(ea);

	/* bring down the ESID */
	esid_val >>= 63 - 35;

	/* FIXME: WHAT the hell is this hack! */
	if (esid_val == 0xc00000000ULL) {
		vsid_val = 0x06a99b4b14;
	} else {
		vsid_val = esid_val;
	}
	return vsid_val;
}

uval
get_pteg(uval vaddr, uval vsid, uval pg_size, uval log_htab_bytes)
{
	uval hash;
	uval vpn = vaddr >> (pg_size);
	uval mask = (1 << (28 - pg_size)) - 1;
	uval htab_bytes = 1UL << log_htab_bytes;
	uval hash_mask = (htab_bytes >> LOG_PTEG_SIZE) - 1;

	hash = (vsid & VSID_HASH_MASK) ^ (vpn & mask);

	return hash & hash_mask;
}

