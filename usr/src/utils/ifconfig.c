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

#include <string.h> /* for memset */
#include <stdio.h>
#include <stdlib.h>
#include <msg.h>
#include <errno.h>

/* ifconfig */
typedef struct {
        int type;
        char interface[4];
				int number;
}Ifconfig_req_t;

typedef struct {
        int type;
        int ret;
}Ifconfig_resp_t;

int main(int argc, char *argv[])
{
  Ifconfig_req_t req;
  Ifconfig_resp_t resp;
  Msg_status_t status;
  int dst;
  int interface;
	char name_num[4];
	char name[3];
	int i,j;
  if(argc>2) {
    printf("Usage: ifconfig [interface]\n");
    exit(EXIT_FAILURE);
  }

	if(argc==1){
	  strncpy( req.interface, "\0", 3 );
	}
	if(argc ==2){
		/*first pass are there more or less than 3 chars */	
		if( strnlen(argv[1], 3) != 3){
			printf("Need proper name\n");
			exit(EXIT_FAILURE);
		}
		/*extract the first 2 no need to malloc because we set the string on init to 2 */		
		strncpy(name, argv[1], 2);
		
		/*should only have these two names */
		i = strncmp("lo", name, 2);
		j = strncmp("et", name, 2);
		if(!(i == 0 || j == 0)){
				printf("Please enter a valid interface name\n");
				exit(EXIT_FAILURE);
		}

		strncpy(name_num, argv[1], 3);
		if(name_num[2] == '0')
			interface = 0;
		else if(name_num[2] == '1')
			interface = 1; 
		else if(name_num[2] == '2')
			interface = 2;
		else if(name_num[2] == '3')
			interface = 3;
		else if(name_num[2] == '4')
			interface = 4;
		else{
				printf("Enter a valid interface number\n");
				exit(EXIT_FAILURE);
		}					
	}
	strncpy(req.interface, name, 3);
  req.number = interface;
	req.type = 5; //NET_IFCONFIG;
  dst = NETD;
  msgsend( dst, &req, sizeof(Ifconfig_req_t) );
  msgrecv( dst, &resp, sizeof(Ifconfig_resp_t), &status );

exit(EXIT_SUCCESS);
  
}

