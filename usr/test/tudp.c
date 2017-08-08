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
 * Simple UDP client/server -- tests network stack using loopback address
 * 
 * Usage: 
 *    ./t6 <-s>  --- run the server
 *    ./t6 -- run the client and generate NMSGS messages
 */

#include <stdio.h>		/* printf */
#include <stdlib.h> 		/* EXIT_FAILURE & EXIT_SUCCESS */
#include <string.h>		/* memset */
#include <inet.h>		/* htons & inet_addr */
#include <socket.h>		/* socket calls */
#include <unistd.h>		/* close */

#define SERV_ADDR "127.0.0.1"
#define UDP_ECHO_PORT 1603
#define BUFFSIZE 255
#define NMSGS 100
#define errorExit(str) { printf(str); exit(EXIT_FAILURE); }

#define TRUE 1
#define FALSE 0

int main(int argc, char **argv) {
  int server, sock, sent,recd, i;
  struct sockaddr_in echoaddr;
  char buf1[BUFFSIZE];
  char buf2[BUFFSIZE];
  unsigned int echolen, addrlen;

  printf("[t6: UDP]");
  fflush(stdout);
  if(argc==2 && (strcmp(argv[1],"-s")==0)) 
    server=TRUE;
  else
    server=FALSE;

  /* Create a UDP socket */
  if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    errorExit("Failed to create socket\n");

  /* Construct sockaddr_in structure */
  memset(&echoaddr, 0, sizeof(echoaddr));	/* Clear struct */
  echoaddr.sin_family = AF_INET;		/* Internet/IP */
  echoaddr.sin_port = htons(UDP_ECHO_PORT);	/* server port */
  addrlen = sizeof(echoaddr);
  
  if(server) {		      
    echoaddr.sin_addr.s_addr = htonl(INADDR_ANY);   /* serve any IP address */
    if (bind(sock,(struct sockaddr *)&echoaddr,sizeof(echoaddr))<0) 
      errorExit("[SERVER} bind failure\n");
    for(i=0; i<NMSGS; i++) {                        /* echo N messages */
      memset(buf1, 0, BUFFSIZE); 
      if ((recd=recvfrom(sock,buf1,BUFFSIZE,0,(struct sockaddr *)&echoaddr,&addrlen))<0)
	errorExit("[SERVER] recvfrom failure\n");
      if ((sent=sendto(sock,buf1,recd,0,
		       (struct sockaddr *)&echoaddr,sizeof(echoaddr))) != recd)
	errorExit("[SERVER] sendto failure\n");
    }
  }  
  else {					     /* be a client */
    echoaddr.sin_addr.s_addr = inet_addr(SERV_ADDR); /* communicate with server IP */
    for(i=0; i<NMSGS; i++) {	                     
      sprintf(buf1,"[Test %d]\n",i);                 /* construct a message */
      echolen = strlen(buf1);	                     /*  Send message to server */
      if ((sent=sendto(sock,buf1,echolen,0,
		       (struct sockaddr *)&echoaddr,sizeof(echoaddr))) != echolen) 
	errorExit("[CLIENT] sendto failure\n");
      memset(buf2, 0, BUFFSIZE);                     /* clear the recv buffer */
      if ((recd=recvfrom(sock,buf2,BUFFSIZE,0,
			 (struct sockaddr *)&echoaddr,&addrlen)) != echolen)
	errorExit("[CLIENT] recvfrom failure\n");
      if (strcmp(buf1, buf2) == 0)                   /* sent not recieved ? */
	printf(".");
      else {
	printf("[CLIENT] %s != %s\n", buf1, buf2);   /* recv != sent */
	return(EXIT_FAILURE);
      }
    }
  }
  printf("\n");
  close(sock);
  return EXIT_SUCCESS;
}



