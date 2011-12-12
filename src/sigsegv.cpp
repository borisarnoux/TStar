/**
 * This source file is used to print out a stack-trace when your program
 * segfaults. It is relatively reliable and spot-on accurate.
 *
 * This code is in the public domain. Use it as you see fit, some credit
 * would be appreciated, but is not a prerequisite for usage. Feedback
 * on it's use would encourage further development and maintenance.
 *
 * Due to a bug in gcc-4.x.x you currently have to compile as C++ if you want
 * demangling to work.
 *
 * Please note that it's been ported into my ULS library, thus the check for
 * HAS_ULSLIB and the use of the sigsegv_outp macro based on that define.
 *
 * Author: Jaco Kroon <jaco@kroon.co.za>
 *
 * Copyright (C) 2005 - 2010 Jaco Kroon
 * 
 *
 */

 /* Modified by Boris Arnoux, for integration with ngpsvcanal. All credits to 
   Jaco Kroon. Unquestionably. */


#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif


#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>

#include <memory.h>

#include <dlfcn.h>
/* This structure mirrors the one found in /usr/include/asm/ucontext.h */
typedef struct _sig_ucontext {
 unsigned long     uc_flags;
 struct ucontext   *uc_link;
 stack_t           uc_stack;
 struct sigcontext uc_mcontext;
 sigset_t          uc_sigmask;
} sig_ucontext_t;

void crit_err_hdlr(int sig_num, siginfo_t * info, void * ucontext)
{
 void *             array[50];
 void *             caller_address;
 char **            messages;
 int                size, i;
 sig_ucontext_t *   uc;

 uc = (sig_ucontext_t *)ucontext;

#ifdef __x86_64
#define R_EIP rip
#else
#define R_EIP eip
#endif

 caller_address = (void *) uc->uc_mcontext. R_EIP;

 fprintf(stderr, "signal %d (%s), address is %p from %p\n",
  sig_num, strsignal(sig_num), info->si_addr,
  (void *)caller_address);

 size = backtrace(array, 50);

 /* overwrite sigaction with caller's address */
 array[1] = caller_address;

 messages = backtrace_symbols(array, size);

 /* skip first stack frame (points here) */
 for (i = 1; i < size && messages != NULL; ++i)
 {
  fprintf(stderr, "[bt]: (%d) %s\n", i, messages[i]);
 }

 free(messages);

 exit(EXIT_FAILURE);
}

static void __attribute__((constructor)) setup_sigsegv() {
	struct sigaction action;
	memset(&action, 0, sizeof(action));
        action.sa_sigaction = crit_err_hdlr;
	action.sa_flags = SA_SIGINFO;
	if(sigaction(SIGSEGV, &action, NULL) < 0)
		perror("sigaction");
}
