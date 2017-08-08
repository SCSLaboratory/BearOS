/*
 * kbd.h
 * Designates the keyboard deamon process id and protocol for communcating
 * with the keyboard deamon.
 */
/*
 * protocol:
 * All communcation with the keyboard deamon should send the following 
 * communcations in pairs:
 *
 *   msgsend(KBD,&Getstr_req_t,sizeof(Getstr_req_t)); -- request a string
 *   msgrecv(KBD,&Getstr_resp_t,sizeof(Getstr_resp_t),NULL); -- recieve a string
 */

#define MAX_STR_SZ   80
#define KEYB_GETSTR   1

/* getstr */
typedef struct {
        int type;
} Getstr_req_t;

typedef struct {
        int type;
        int slen;
        char str[MAX_STR_SZ];
} Getstr_resp_t;

