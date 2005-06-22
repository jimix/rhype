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
#include <openfirmware.h>
#include <of_devtree.h>
#include <ofd.h>
#include <test.h>
#include <mmu.h>
#include <hcall.h>


void ofd_platform_probe(void* ofd)
{
	(void)ofd;

	save_mmio_range(0xf0000000, 0x2000000, "PCI");
	save_mmio_range(0xf2000000, 0x2000000, "HT");
	save_mmio_range(0xf4000000, 0x400000, "HT");

	save_mmio_range(0xf8000000, 0x80000, "U3");
	save_mmio_range(0xff800000, 0x80000, "ROM");
	save_mmio_range(0xf9000000, 0x100000, "vsp");

	save_mmio_range(0xfff04000, 0x4000, "NVRAM");
	save_mmio_range(0x80000000, 0x80000, "macio");
	save_mmio_range(0x80600000, 0x10000, "k2-sata");

	save_mmio_range(0x90000000, 0x40000, "tg3");

	/* G5's have a little-endian openpic */
	ofd_openpic_probe(ofd, 1);


	/* Define the ibm,dma-window corresponding to a complete DART */
	uval32 ofd_pci_dma_window[] = { 0, 0, 0, 0, 0, 1<<31 };
	const char device_tree[] = "device-tree";

	ofdn_t n = ofd_node_find_by_prop(ofd, -1, "name",
					 device_tree, sizeof(device_tree));
	if (n > 0)
		ofd_prop_add(ofd, n, "ibm,dma-window",
			     &ofd_pci_dma_window, sizeof (ofd_pci_dma_window));

}
