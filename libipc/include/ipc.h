/*
 * Copyright (C) 2006 Ondrej Palkovsky
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

#ifndef __LIBIPC_IPC_H__
#define __LIBIPC_IPC_H__

#include <kernel/ipc/ipc.h>
#include <libc.h>

typedef sysarg_t ipcarg_t;
typedef ipcarg_t ipc_data_t[IPC_CALL_LEN];
typedef sysarg_t ipc_callid_t;

typedef void (* ipc_async_callback_t)(void *private, 
				      int retval,
				      ipc_data_t *data);

#define ipc_call_sync_2(phoneid, method, arg1, arg2, res1, res2) ipc_call_sync_3((phoneid), (method), (arg1), (arg2), 0, (res1), (res2), 0)
extern int ipc_call_sync_3(int phoneid, ipcarg_t method, ipcarg_t arg1,
			   ipcarg_t arg2, ipcarg_t arg3,
			   ipcarg_t *result1, ipcarg_t *result2, 
			   ipcarg_t *result3);


extern int ipc_call_sync(int phoneid, ipcarg_t method, ipcarg_t arg1, 
			 ipcarg_t *result);
extern int ipc_wait_for_call(ipc_data_t *data, int flags);
extern void ipc_answer(ipc_callid_t callid, ipcarg_t retval, ipcarg_t arg1,
		       ipcarg_t arg2);

#define ipc_call_async(phoneid,method,arg1,private, callback) (ipc_call_async_2(phoneid, method, arg1, 0, private, callback))
void ipc_call_async_2(int phoneid, ipcarg_t method, ipcarg_t arg1,
		      ipcarg_t arg2, void *private,
		      ipc_async_callback_t callback);

#endif
