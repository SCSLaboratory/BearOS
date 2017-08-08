#pragma once
/*******************************************************************************
 *
 * File: mptest.h
 *
 * Description: Contains message definitions used by the programs a and b to
 *     test message-passing between processes.
 *
 ******************************************************************************/

/* 
 * Astute observers will notice that this is very similar to the old fixed-
 *  length messages we used to have.  That's because I didn't feel like
 *  changing a and b.
 */
typedef struct {
	int type;
	int i1;
	int i2;
	int i3;
	uint8_t *p1;
	uint8_t *p2;
	uint8_t *p3;
} Test_msg_t;


