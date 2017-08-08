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
#include <stdio.h>
#include <stdlib.h>		/* EXIT_FAILURE */
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define MAXSTR 256

main(int argc, char *argv[])
{
  struct in_addr addr;
  char straddr[MAXSTR];

  strcpy(straddr, "226.000.000.037"); /* last byte in octal */
  inet_aton(straddr, &addr);
  if(strcmp(inet_ntoa(addr),"226.0.0.31")!=0)
    exit(EXIT_FAILURE);

  strcpy(straddr, "0x7f.1");	/* first byte in hex */
  inet_aton(straddr, &addr);
  if(strcmp(inet_ntoa(addr),"127.0.0.1")!=0)
    exit(EXIT_FAILURE);

  exit(EXIT_SUCCESS);
}
