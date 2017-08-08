/*
 Copyright <2017> <Scaleable and Concurrent Systems Lab; 
                   Thayer School of Engineering at Dartmouth College>

 Permission is hereby granted, free of charge, to any person obtaining a copy 
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights 
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
 copies of the Software, and to permit persons to whom the Software is 
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
*/
#include <jobs.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>

unsigned int   jid_ctr;

sigset_t sigchld_wait_set;

int fg_pid;
char fg_command[1000];
int fg_retval;

job_list_t *list;
job_list_t *plist;

#define first list->head
#define second (list->head ? list->head->next : (job_t*)~0x0)

void sigchld_handler(int signo){

  job_t *j;
  int pid, status;

  pid = waitpid(-1, &status, WNOHANG|WUNTRACED);

  j = find_job(0, pid);
  
  if ( pid == 0 ) {
    return;
  }
  else if ( !j ) {
    if ( WIFEXITED(status) ) {
      fg_retval = WEXITSTATUS(status);
    }
    else if ( WIFSTOPPED(status ) ){
      add_job(0,0,pid,0);
      find_job(0,pid)->state = JOB_STOPPED;
    }
    fg_pid = 0;
  }
  else {
    if ( WIFEXITED(status) ) {
      j->state = JOB_EXITED;
      j->exit_stat = WEXITSTATUS(status);
      remove_job(j->jid, j->pid);
    }
    else if ( WIFSIGNALED(status) ) {
      j->state = JOB_KILLED;
      remove_job(j->jid, j->pid);
    }
    else if ( WIFSTOPPED(status) ) 
      j->state = JOB_STOPPED;
    
    fg_pid = 0;
  }

  return;
}

void signal_handler(int signo) {

  if ( fg_pid ) 
    kill(fg_pid, signo);

  if ( fg_pid ) 
    sigsuspend(&sigchld_wait_set);

  return;
}

int init_job_ctrl(void) {

  jid_ctr = 1;

  list = (job_list_t*)malloc(sizeof(job_list_t));
  if ( !list ) 
    return -1;
  list->head = list->tail = NULL;
  list->size = 0;

  plist = (job_list_t*)malloc(sizeof(job_list_t));
  if ( !plist ) 
    return -1;
  plist->head = plist->tail = NULL;
  plist->size = 0;
  
  sigemptyset(&sigchld_wait_set);

  signal(SIGCHLD, sigchld_handler);

  signal(SIGINT, signal_handler);
  signal(SIGTSTP, signal_handler);

  return 0;
}

int resume(int argc, char **argv, int fg) {

  job_t *j;

  if ( argc < 2 ) 
    j = first;
  else 
    j = find_job(atoi(&argv[1][1]), 0);

  if ( !j ) {
    printf("No such job.\n");
    return -1;
  }

  if ( j->state != JOB_RESUMED )
    kill(j->pid, SIGCONT);

  j->state = JOB_RESUMED;

  move_to_front(j->jid, j->pid);
  if ( fg ) {
    fg_pid = j->pid;
    sigsuspend(&sigchld_wait_set);
  }

  return 0;
}

void print_jobs() {
  
  job_t *j;
  int killme = 0;

  for ( j = plist->head ; j ; j = j->print_next ) {
    printf("[%2d]", j->jid);

    if ( j == first )
      printf("+");
    else if ( j == second )
      printf("-");
    else 
      printf(" ");

    printf("  ");
    if ( j->state == JOB_STOPPED ) 
      printf("Stopped");
    else if ( j->state == JOB_RESUMED )
      printf("Running");
    else if ( j->state == JOB_KILLED ) {
      printf("Killed ");
      killme = 1;
    }
    else if ( j->state == JOB_EXITED ) { 
      printf("Exited ");
      killme = 1;
    }

    printf("    ");

    printf("%s\n", j->command);

    fflush(stdout);
    
    /* if the proc isnt dead */
    if ( !killme )
      continue;

    killme = 0;
    remove_job_print(j->jid, j->pid);
  }

  return;
}

int add_job(int argc, char **argv, int pid, int fg) {
  
  char *ptr;
  int i, len;
  job_t *j;

  if ( argc ) { /* its not the foreground command */

    /* get the command from the actual argc argv */

    len = 0;
    for ( i = 0; i < argc; i++ ) 
      len += strlen(argv[i]);
    
    len += argc; /* for spaces and null terminator */

    /* where are we going to store the command? */
    if ( !fg ) {
      j = (job_t*)malloc(sizeof(job_t));
      if ( !j )
	return -1;
      ptr = j->command = (char*)malloc(len * sizeof(char));
    }
    else 
      ptr = fg_command;
    
    /* store the command in one string */
    for ( i = 0 ; i < argc ; i++ ) {
      strcpy(ptr, argv[i]);
      ptr += strlen(argv[i]);
      *ptr = ' ';
      ptr++;
    }
    *(--ptr) = '\0';
  }

  if ( fg ) {
    
    fg_pid = pid;

    sigsuspend(&sigchld_wait_set);

    return fg_retval;
  }
  
  j->pid = pid;
  j->jid = jid_ctr++;
  j->state = JOB_RESUMED;
  j->prev = j->next = NULL;
  j->print_prev = j->print_next = NULL;

  /* add to head of list */
  if ( list->head ) 
    list->head->prev = j;
  j->next = list->head;
  list->head = j;
  if ( !list->tail )
    list->tail = j;
  list->size++;

  /* add to tail of print list */
  if ( plist->tail ) 
    plist->tail->print_next = j;
  j->print_prev = plist->tail;
  plist->tail = j;
  if ( !plist->head )
    plist->head = j;
  plist->size++;

  return EXIT_SUCCESS;
}

int remove_job_print(int jid, int pid) {
  
  job_t *j;

  j = find_job(jid, pid);

  if ( !j ) 
    return -1;

  /* reclaim job ID if I should */
  if ( j == plist->tail )
    jid_ctr = j->print_prev ? j->print_prev->jid + 1 : 1;

  /* from the print list */
  if ( j->print_prev ) 
    j->print_prev->print_next = j->print_next;
  else 
    plist->head = j->print_next;

  if ( j->print_next ) 
    j->print_next->print_prev = j->print_prev;
  else 
    plist->tail = j->print_prev;

  plist->size--;

  free(j->command);
  free(j);

  return 0;
}

int remove_job(int jid, int pid) {
  
  job_t *j;

  j = find_job(jid, pid);

  if ( !j ) 
    return -1;

  /* from the actual list */
  if ( j->prev ) 
    j->prev->next = j->next;
  else 
    list->head = j->next;

  if ( j->next ) 
    j->next->prev = j->prev;
  else 
    list->tail = j->prev;
  
  list->size--;

  return 0;
}

static job_t *find_job(int jid, int pid) {

  job_t *j;

  j = plist->head;
  if ( !j ) 
    return NULL;

  do {

    if ( (jid ? j->jid == jid : j->pid == pid) )
      return j;

    j = j->print_next;
  } while ( j != NULL ) ;
  
  return NULL;
}

static int move_to_front(int jid, int pid) {
  
  job_t *j;
  
  /* find job */
  j = find_job(jid, pid);
  if ( !j ) 
    return -1;
  
  /* remove from list */
  if ( j->prev ) 
    j->prev->next = j->next;
  else 
    list->head = j->next;
  if ( j->next ) 
    j->next->prev = j->prev;
  else
    list->tail = j->prev;
  
  /* add to head */
  j->prev = NULL;
  j->next = list->head;
  if ( list->head )
    list->head->prev = j;
  list->head = j;
  
  return 0;
}
