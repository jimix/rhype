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

#ifndef _HYPE_POWERPC_HYPE_CALLS_H
#define _HYPE_POWERPC_HYPE_CALLS_H

#include_next <h_proto.h>

extern sval h_rtas(struct cpu_thread *, uval argp, uval privp);
extern sval h_htab(struct cpu_thread *thread, uval lpid, uval log_size);
extern sval h_set_spr(struct cpu_thread *thread, uval reg, uval value);
extern sval h_vrfid(struct cpu_thread *thread, uval restore_idx, uval new_dec);
extern sval h_sync_vm_class(struct cpu_thread *thread);


extern sval h_destroy_vm_class(struct cpu_thread *thread, uval id);
extern sval h_create_vm_class(struct cpu_thread *thread,
			      uval type, uval id, uval ea_base, uval size,
			      uval imp_arg1, uval imp_arg2,
			      uval imp_arg3, uval imp_arg4);

extern sval h_vm_class_op(struct cpu_thread *thread, uval id,
			  uval arg1, uval arg2, uval arg3, uval arg4);
#endif /* ! _HYPE_POWERPC_HYPE_CALLS_H */
