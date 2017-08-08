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
#include <stdlib.h>

#define MAXBUFF 256

int main(void) {
  char buff[MAXBUFF];
  FILE *fh;

  printf("> ");
  fflush(stdout);
  scanf("%s",buff);
  printf("[%s]\n",buff);

  printf("> ");
  fflush(stdout);
  scanf("%s",buff);
  printf("[%s]\n",buff);

  printf("> ");
  fflush(stdout);
  scanf("%s",buff);
  printf("[%s]\n",buff);

  if((fh=fopen("/bin/tests","r"))<0) {
    printf("Unable to open /bin/tests\n");
    exit(EXIT_FAILURE);
  }
  fscanf(fh,"%s",buff);
  if(strcmp(buff,"#")!=0)
    exit(EXIT_FAILURE);
  printf("[%s]\n",buff);

  fscanf(fh,"%s",buff);
  if(strcmp(buff,"command")!=0)
    exit(EXIT_FAILURE);
  printf("[%s]\n",buff);

  fscanf(fh,"%s",buff);
  if(strcmp(buff,"line")!=0)
    exit(EXIT_FAILURE);
  printf("[%s]\n",buff);

  fclose(fh);
  
  exit(EXIT_SUCCESS);
}
