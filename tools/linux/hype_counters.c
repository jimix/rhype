/*
 * Copyright (C) 2005 Michal Ostrowski <mostrows@watson.ibm.com>, IBM Corp.
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

#include <hype.h>
#include <hype_util.h>
#include <util.h>
#include <partition.h>
#include <hypervisor.h>
#include <hcounters.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct counter
{
	uval id;
	const char* name;
};

struct counter names[] = {
#include "counter_names.h"
	{ 0, NULL}
};


static int
name_to_counter(const char *name)
{
	int x = 0;
	while (names[x].name && strcmp(names[x].name, name)) {
		++x;
	}
	return names[x].id;
}

static const char*
counter_to_name(uval id)
{
	int x = 0;
	while (names[x].name && names[x].id != id) {
		++x;
	}
	return names[x].name;
}



static int
freeze(void)
{
	oh_hcall_args hargs;
	hargs.opcode = H_DEBUG;
	hargs.args[0] = H_COUNTER_FREEZE;

	int ret = hcall(&hargs);

	ASSERT(ret >= 0 && hargs.retval == 0,
	       "hcall failure: %d " UVAL_CHOOSE("0x%x", "0x%lx") "\n", ret,
	       hargs.retval);
	return ret;
}

static int
thaw(void)
{
	oh_hcall_args hargs;
	hargs.opcode = H_DEBUG;
	hargs.args[0] = H_COUNTER_THAW;

	int ret = hcall(&hargs);

	ASSERT(ret >= 0 && hargs.retval == 0,
	       "hcall failure: %d " UVAL_CHOOSE("0x%x", "0x%lx") "\n", ret,
	       hargs.retval);
	return ret;
}


static int
set_counters(int argc, char **argv)
{
	oh_hcall_args hargs;
	hargs.opcode = H_DEBUG;
	hargs.args[0] = H_COUNTER_SET;
	uval idx = 0;
	while (argc) {
		uval id = name_to_counter(argv[0]);

		if (!id)
			id = strtoul(argv[0], NULL, 0);


		hargs.args[1] = idx++;
		hargs.args[2] = id;

		int ret = hcall(&hargs);

		ASSERT(ret >= 0 && hargs.retval == 0,
		       "hcall failure: %d " UVAL_CHOOSE("0x%x", "0x%lx") "\n",
		       ret, hargs.retval);
		--argc;
		++argv;
	}
	return 0;
}

static int
get_counters(void)
{
	oh_hcall_args hargs;
	uval idx = 0;
	while (idx < 16) {
		hargs.opcode = H_DEBUG;
		hargs.args[0] = H_COUNTER_GET;
		hargs.args[1] = idx;

		int ret = hcall(&hargs);

		ASSERT(ret >= 0 && hargs.retval == 0,
		       "hcall failure: %d " UVAL_CHOOSE("0x%x", "0x%lx") "\n",
		       ret, hargs.retval);

		if (hargs.args[1]) {
			const char* name = counter_to_name(hargs.args[1]);
			if (name) {
				printf("%2lx: %28s %6ld\n",
				       idx, name, hargs.args[0]);
			} else {
				printf("%2lx: %28lx %6ld\n",
				       idx, hargs.args[1], hargs.args[0]);
			}
		}



		++idx;
	}
	return 0;
}

int
main(int argc, char **argv)
{
	if (argc == 0) return -1;
	hcall_init();
	--argc;
	++argv;
	if (strcmp("freeze", argv[0]) == 0) {
		return freeze();
	} else if (strcmp("thaw", argv[0]) == 0) {
		return thaw();
	} else if (strcmp("set", argv[0]) == 0) {
		return set_counters(--argc, ++argv);
	} else if (strcmp("get", argv[0]) == 0) {
		return get_counters();
	}
	return -1;

}

