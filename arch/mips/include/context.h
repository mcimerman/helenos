/*
 * Copyright (C) 2001-2004 Jakub Jermar
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

#ifndef __mips_CONTEXT_H__
#define __mips_CONTEXT_H__

#include <arch/types.h>

#define STACK_ITEM_SIZE	4

/*
 * Put one item onto the stack to support get_stack_base().
 */
#define SP_DELTA	(0+STACK_ITEM_SIZE)


struct context {
	__u32 r0;
	__u32 r1;
	__u32 r2;
	__u32 r3;
	__u32 r4;
	__u32 r5;
	__u32 r6;
	__u32 r7;
	__u32 r8;
	__u32 r9;
	__u32 r10;
	__u32 r11;
	__u32 r12;
	__u32 r13;
	__u32 r14;
	__u32 r15;
	__u32 r16;
	__u32 r17;
	__u32 r18;
	__u32 r19;
	__u32 r20;
	__u32 r21;
	__u32 r22;
	__u32 r23;
	__u32 r24;
	__u32 r25;
	__u32 r26;
	__u32 r27;
	__u32 r28;
	__u32 sp;
	__u32 r30;
	__u32 pc;
	__u32 pri;
};

#endif
