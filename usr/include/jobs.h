/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)jobs.h	8.2 (Berkeley) 5/4/95
 * $FreeBSD: src/bin/sh/jobs.h,v 1.18 2004/04/06 20:06:51 markm Exp $
 */
#include <signal.h>

#define JOB_STOPPED 1
#define JOB_RESUMED 2
#define JOB_EXITED  3
#define JOB_KILLED  4

typedef struct _job {
  pid_t pid;
  unsigned int jid;
  char state;		/* true if job is finished */
  char *command;
  int exit_stat;
  struct _job *next;	/* job used after this one */
  struct _job *prev;
  struct _job *print_next;
  struct _job *print_prev;
} job_t;

typedef struct job_ll {
  job_t *head;
  job_t *tail;
  int    size;
} job_list_t;

int init_job_ctrl(void);
void init_job_list(job_list_t *);
int add_job(int, char**,int, int);
int resume(int, char **, int);

int remove_job(int jid, int pid);
int remove_job_print(int jid, int pid);

void print_jobs(void);

static job_t *find_job(int,int);
static int move_to_front(int jid, int pid);
