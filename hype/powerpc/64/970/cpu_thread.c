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
#include <cpu_thread.h>
#include <pmm.h>

void
imp_thread_init(struct cpu_thread *thread)
{
	uval base = thread->cpu->os->rmo;
	uval size = thread->cpu->os->rmo_size;

	thread->imp_regs.hid4.word = get_hid4();
	if (base != INVALID_PHYSICAL_ADDRESS) {
		assert(cpu_verify_rmo(base, size), "Invalid rmo, rmo size\n");

#ifdef FORCE_APPLE_MODE
	} else {
		thread->imp_regs.hid4.bits.lpes0 = 0;
		thread->imp_regs.hid4.bits.lpes1 = 0;

		vbase_config(thread, 0, SEGMENT_SIZE);

		uval i = 0;
		thread->vregs->active_vsave = 0;

		for (; i < 12; ++i) {
			thread->vregs->vexc_save[0].reg_gprs[i] =
				thread->reg_gprs[i];
		}

		thread->reg_srr1 = V_LPAR_MSR_ON;
		thread->vregs->v_msr = (V_LPAR_MSR_ON | MSR_AM) & ~MSR_PR;
#endif
	}

}

void
imp_switch_to_thread(struct cpu_thread *thread)
{
	union hid4 hid4;
	uval rmo_size = thread->cpu->os->rmo_size;

	hid4 = thread->imp_regs.hid4;
#ifdef FORCE_APPLE_MODE
	hid4.bits.rmor = 0;
	rmo_size = 1ULL << 30;
#else
	hid4.bits.rmor = thread->cpu->os->rmo >> 26;
#endif

	hid4.bits.lpid01 = thread->cpu->os->po_lpid_tag & 3;
	hid4.bits.lpid25 = (thread->cpu->os->po_lpid_tag >> 2) & 0xf;

	switch (rmo_size) {
	default:
	case 256ULL << 30:	/* 256 GB */
		hid4.bits.rmlr0 = 0;
		hid4.bits.rmlr12 = 0;
		break;
	case 16ULL << 30:	/* 16 GB */
		hid4.bits.rmlr0 = 0;
		hid4.bits.rmlr12 = 1;
		break;
	case 1ULL << 30:	/* 1 GB */
		hid4.bits.rmlr0 = 0;
		hid4.bits.rmlr12 = 2;
		break;
	case 64ULL << 20:	/* 64 MB */
		hid4.bits.rmlr0 = 0;
		hid4.bits.rmlr12 = 3;
		break;
	case 256ULL << 20:	/* 256 MB */
		hid4.bits.rmlr0 = 1;
		hid4.bits.rmlr12 = 0;
		break;
	case 128ULL << 20:	/* 128 MB */
		hid4.bits.rmlr0 = 1;
		hid4.bits.rmlr12 = 3;
		break;
	}


	if (hid4.word != thread->imp_regs.hid4.word) {
		set_hid4(hid4.word);
	}
	hid4.word = get_hid4();
}
