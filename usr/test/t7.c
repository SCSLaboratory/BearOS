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
/*
 * Simple TCP client/server -- tests network stack using loopback address
 * 
 * Usage: 
 *    ./t7 <-s>  --- run the server and echo NMSGS
 *    ./t7 -- run the client and generate NMSGS
 */

#include <inttypes.h>		/* PRIu64 */
#include <stdio.h>		/* printf */
#include <stdlib.h> 		/* EXIT_FAILURE & EXIT_SUCCESS */
#include <string.h>		/* memset */
#include <sys/socket.h>		/* socket calls */
#include <arpa/inet.h>		/* htons & inet_addr */
#include <unistd.h>		/* close */

//#define TTCP_DEBUG 1		/* print out debug info */

#ifdef TTCP_DEBUG
#define NMSGS 3			/* number of messages */
#else
#define NMSGS 1000	        /* number of messages */
#endif	/* TTCP_DEBUG */

#define ONEK (1024)             /* sizeof each message */

#define SERV_ADDR_HW "192.168.0.151"
#define SERV_ADDR_VM "192.168.0.152"
#define SERV_ADDR_LB "127.0.0.1"

#define TCP_ECHO_PORT 1604
#define LISTENQ (16)
#define errorExit(str) { printf(str); exit(EXIT_FAILURE); }


void checksum(char *str, size_t size, uint64_t *buffer);
void sendall(int sock,uint64_t *buffer,size_t num);
void recvall(int sock,uint64_t *buffer,size_t num);

int main(int argc, char **argv) {
  int sock,cval,client_sock;		/*  listening socket */
  struct sockaddr_in servaddr;  /*  socket address structure */
  uint64_t *bp,*bp1,*buffer,*buffer1;
  size_t msgsize,msgsizebytes;
  int i,j,k;

  printf("[t7: TCP]\n");
  fflush(stdout);
  if(argc==2 && (strcmp(argv[1],"-s")==0)) {
    /*  Create a TCP socket  */
    if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0 )
      errorExit("SERVER: error creating listening socket.\n");
    /* set up the server address */
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port   = htons(TCP_ECHO_PORT);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    /*  Bind socket addresss to listening socket, and listen  */
    if (bind(sock, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0 ) 
      errorExit("SERVER: error calling bind\n");
    if (listen(sock, LISTENQ) < 0 )
      errorExit("SERVER: error calling listen\n");
#ifdef TTCP_DEBUG
    printf("SERVER: Listening on socketid %d\n",sock);
#endif	/* TTCP_DEBUG */
    /* echo NMSGS */
    for(i=1; i<=NMSGS; i++) {
#ifdef TTCP_DEBUG
      printf(".");
      fflush(stdout);
#endif	/* TTCP_DEBUG */
      msgsize=(i*ONEK);
      msgsizebytes=(msgsize*sizeof(uint64_t));

     /* create an echo buffer */
      if((buffer=(uint64_t*)malloc(msgsizebytes))==NULL)
	errorExit("SERVER: malloc error\n");

      /*  accept client_socks and echo any message recieved */
      if((client_sock = accept(sock, NULL, NULL) ) < 0 ) 
	errorExit("SERVER: error calling accept\n");
      recvall(client_sock,buffer,msgsizebytes);
      sendall(client_sock,buffer,msgsizebytes);
      /* close the client_sock and socket */
      if (close(client_sock) < 0 ) 
	errorExit("SERVER: error calling closing client_sock\n");
      free(buffer);
    }
    /* close the listening socket */
    if(close(sock) <0)
      errorExit("error closing socket\n");
  }
  else {
    /* set up the server address */
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port   = htons(TCP_ECHO_PORT);
    if(argc==2 && (strcmp(argv[1],"-vm")==0)) {
      if(inet_aton(SERV_ADDR_VM,&(servaddr.sin_addr)) <= 0)
	errorExit("CLIENT: error on inet_aton\n");
    }
    else if(argc==2 && (strcmp(argv[1],"-hw")==0)) {
      if(inet_aton(SERV_ADDR_HW,&(servaddr.sin_addr)) <= 0)
	errorExit("CLIENT: error on inet_aton\n");
    }
    else if(argc==2 && (strcmp(argv[1],"-lb")==0)) {
      if(inet_aton(SERV_ADDR_LB,&(servaddr.sin_addr)) <= 0)
	errorExit("CLIENT: error on inet_aton\n");
    }
    else if(argc==3 && (strcmp(argv[1],"-ip")==0)) {
      if(inet_aton(argv[2],&(servaddr.sin_addr)) <= 0)
	errorExit("CLIENT: error on inet_aton\n");
    }
    else {
	errorExit("Usage: t7 -[lb,hw,vm,ip <IP>]\n");
    }
    /* send and recieve NMSGS */
    for(i=1; i<=NMSGS; i++) {
#ifdef TTCP_DEBUG
      printf("#");
      fflush(stdout);
#endif	/* TTCP_DEBUG */
      msgsize=(i*ONEK);
      msgsizebytes=(msgsize*sizeof(uint64_t));
      /* client sends from buffer, recieves to buffer1 */
      if((buffer=(uint64_t*)malloc(msgsizebytes))==NULL)
	errorExit("CLIENT: malloc error\n");
      if((buffer1=(uint64_t*)malloc(msgsizebytes))==NULL)
	errorExit("CLIENT: malloc error\n");
      /* client initializes the send and recv buffer */
      for(bp=buffer,bp1=buffer1,j=0; j<msgsize; j++, bp++,bp1++) {
	*bp=j;
	*bp1=0;
      }
      /* create the socket */
      if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0 )
	errorExit("CLIENT: socket error - unable to create socket\n");

      /* connect to the server */
#ifdef TTCP_DEBUG
      printf("CLIENT: socket created, i=%d, sock=%d\n",i,sock);
#endif	/* TTCP_DEBUG */
      if((cval=connect(sock,(struct sockaddr *) &servaddr, sizeof(servaddr))) < 0)
	errorExit("CLIENT: connect error - server not responding\n");
#ifdef TTCP_DEBUG
      printf("CLIENT: connect returns %d\n",cval);
#endif	/* TTCP_DEBUG */


      /* send and recieve */
      sendall(sock,buffer,msgsizebytes);
      recvall(sock,buffer1,msgsizebytes);

      /* check sent was recvd */
      for(bp=buffer,bp1=buffer1,j=0; j<msgsize; j++,bp++,bp1++) 
	if(*bp!=*bp1) {
	  printf("CLIENT: %d:%"PRIu64",%"PRIu64"\n",(int)j,*bp,*bp1);
	  errorExit("CLIENT: messages do not compare\n");
	}

      /* close the socket */
      free(buffer);
      free(buffer1);
      if(close(sock) <0)
	errorExit("error closing socket\n");
      if(i%10==0) {
	printf(".");
	fflush(stdout);
      }
      if(i%500==0)
	printf("%d\n", i);
    }
  }
  printf("\n");
  return (EXIT_SUCCESS);
}

void checksum(char *str, size_t size, uint64_t *buffer) {
  uint64_t *bp;
  size_t i;
  double checksum;
  for(checksum=0, bp=buffer, i=0; i<size; i++,bp++)
    checksum += *bp;
  printf("%s: size=%d,checksum=%lf\n",str,(int)size,checksum);
}

void sendall(int sock,uint64_t *buffer,size_t num) {
  
  uint8_t *bp;
  ssize_t nsent;
  bp=(uint8_t*)buffer;
  while(num>0) {
    if((nsent = send(sock,(void*)bp,num,0))<0) {
      printf("error on send: %d\n",(int)nsent);
      exit(EXIT_FAILURE);
    }
    bp += nsent;
    num -= nsent;
  }
}

void recvall(int sock,uint64_t *buffer,size_t num) {
  uint8_t *bp;
  size_t left;
  ssize_t nrecv;
  bp = (uint8_t*)buffer;
  left = num;
  while(left>0) {
    if((nrecv = recv(sock,(void*)bp,left,0))<0) {
      printf("error on recv: %d\n",(int)nrecv);
      exit(EXIT_FAILURE);
    }
    bp += nrecv;
    left -= nrecv;
  }
}

