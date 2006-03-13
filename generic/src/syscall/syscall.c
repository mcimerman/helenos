/*
 * Copyright (C) 2005 Martin Decky
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <syscall/syscall.h>
#include <proc/thread.h>
#include <print.h>
#include <putchar.h>
#include <ipc/ipc.h>
#include <errno.h>
#include <proc/task.h>
#include <arch.h>
#include <debug.h>

static __native sys_ctl(void) {
	printf("Thread finished\n");
	thread_exit();
	/* Unreachable */
	return 0;
}

static __native sys_io(int fd, const void * buf, size_t count) {
	
	// TODO: buf sanity checks and a lot of other stuff ...

	size_t i;
	
	for (i = 0; i < count; i++)
		putchar(((char *) buf)[i]);
	
	return count;
}

/** Send a call over syscall
 *
 * @return Call identification, returns -1 on fatal error, 
           -2 on 'Too many async request, handle answers first
 */
static __native sys_ipc_call(__native phoneid, __native arg1, __native arg2)
{
	call_t *call;
	phone_t *phone;

	if (phoneid >= IPC_MAX_PHONES)
		return -ENOENT;

	phone = &TASK->phones[phoneid];
	if (!phone->callee)
		return -ENOENT;


	/* TODO: Check that we did not exceed system imposed maximum
	 * of asynchrnously sent messages
	 * - the userspace should be able to handle it correctly
	 */
	call = ipc_call_alloc();
	call->data[0] = arg1;
	call->data[1] = arg2;
	ipc_call(phone, call);

	return (__native) call;
}

/** Send IPC answer */
static __native sys_ipc_answer(__native callid, __native arg1, __native arg2)
{
	call_t *call;

	/* Check that the user is not sending us answer callid */
	ASSERT(! (callid & 1));
	/* TODO: Check that the callid is in the dispatch table */
	call = (call_t *) callid;

	call->data[0] = arg1;
	call->data[1] = arg2;

	ipc_answer(&TASK->answerbox, call);
	return 0;
}

/** Wait for incoming ipc call or answer
 *
 * @param result 
 * @param flags
 * @return Callid, if callid & 1, then the call is answer
 */
static __native sys_ipc_wait_for_call(__native *calldata, __native flags)
{
	call_t *call;
	
	call = ipc_wait_for_call(&TASK->answerbox, flags);
	copy_to_uspace(calldata, &call->data, sizeof(__native) * IPC_CALL_LEN);

	if (call->flags & IPC_CALL_ANSWERED)
		return ((__native)call) | 1;
	return (__native)call;
}


syshandler_t syscall_table[SYSCALL_END] = {
	sys_ctl,
	sys_io,
	sys_ipc_call,
	sys_ipc_answer,
	sys_ipc_wait_for_call
};
