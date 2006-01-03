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

/*
 * Example of how to do AIPC from a user application.
 */

#include <hype.h>
#include <hype_util.h>

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <signal.h>

oh_reflect_args ora;
oh_hcall_args args;

/* Define a set of messages for TestOS's to use */
enum {
	IPC_REGISTER_SELF = 0x0,	/* tell controller we'll accept msgs */
	IPC_LPAR_RUNNING = 0x1,	/* tell OS about existence of another */
	IPC_LPAR_DEAD = 0x2,	/* notification of LPAR death */
	IPC_SUICIDE = 0x3,	/* ask controller to kill me */
	IPC_RESOURCES = 0x4,	/* resources have been given */
};

#define MAX_NUM_AIPC	16	/* Number of aipc bufs per partition */

#define AIPC_LOCAL	1

struct async_msg_s {
	hval am_source;
	union {
		struct {
			hval am_mtype;
			union {
				struct {
					hval amr_lpid;
				} amt_running;
				struct {
					hval amd_lpid;
				} amt_dead;
				struct {
					hval ams_type;
					hval ams_laddr;
					hval ams_size;
				} amt_resources;
			} amt_data;
		};
		hval amu_data[4];
	} am_data;
};

struct msg_queue {
	hval bufSize;
	hval head;
	hval tail;
	struct async_msg_s buffer[0];
};

static void
msg_sig_fn(int sig, siginfo_t * si, void *ptr)
{
	(void)si;
	printf("Received signal: %d %p\n", sig, ptr);
}

struct sigaction msg_sig = {
	.sa_sigaction = msg_sig_fn,
	.sa_flags = SA_SIGINFO,

};

int
main(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	int fd = hcall_init();

	hval laddr = mem_hold(4096);
	char *ptr = mmap(NULL, 4096,
			 PROT_READ | PROT_WRITE, MAP_SHARED,
			 fd, laddr);

	args.opcode = H_CREATE_MSGQ;
	args.args[0] = laddr;
	args.args[1] = 4096;
	args.args[2] = 0;

	hcall(&args);

	hval xirr = args.args[0];

	printf("hcall: " HVAL " (" HVAL ") " HVAL " " HVAL "\n",
	       args.retval, xirr, args.args[1], args.args[2]);

	sigfillset(&msg_sig.sa_mask);
	sigaction(SIGRTMIN, &msg_sig, NULL);

	ora.oh_interrupt = xirr;
	ora.oh_signal = SIGRTMIN;
	ioctl(fd, OH_IRQ_REFLECT, &ora);

	struct msg_queue *mq = (struct msg_queue *)ptr;

	printf("msgq: " HVAL " "  HVAL " " HVAL "\n",
	       mq->bufSize, mq->head, mq->tail);

	uval lpid;

	args.opcode = H_GET_LPID;
	hcall(&args);
	lpid = args.args[0];
	printf("self lpid: %lx\n", lpid);

	while (1) {
		sigset_t set;

		sigemptyset(&set);
		sigaddset(&set, SIGRTMIN);

		siginfo_t info;
		int ret = sigwaitinfo(&set, &info);

		printf("sigwaitinfo: %d\n", ret);
		printf("msgq: " HVAL " " HVAL" " HVAL "\n",
		       mq->bufSize, mq->head, mq->tail);

		int tail = mq->tail % mq->bufSize;
		struct async_msg_s *msg = &mq->buffer[tail];

		printf("msg from: " HVAL "\n", msg->am_source);
		printf("msg raw data: " HVAL " " HVAL " " HVAL " " HVAL "\n",
		       msg->am_data.amu_data[0], msg->am_data.amu_data[1],
		       msg->am_data.amu_data[2], msg->am_data.amu_data[3]);

		++mq->tail;
	}

	close(fd);
	return 0;
}
