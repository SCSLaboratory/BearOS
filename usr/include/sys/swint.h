#pragma once
#ifndef _IN_ASM
/* 
 * This is it.  This is used by all processes when communicating. A process
 * can do one of three things: send, receive, or both.  All calls found in
 * the user library are both.  This is so that a user's code doesn't start up
 * again immediately once a message is received; the user process must wait
 * until its request has been serviced before it runs again.  Receiving a
 * message containing either the requested resource/value, or receiving a
 * message with OK/Error message accomplishes this.
*/
int swint(int, void*);
void swint_2();

#endif
