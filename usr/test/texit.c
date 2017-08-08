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
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
  int ret;

  if(argc!=2) {
    printf("Usage: tex [1,2,3,4] -- 1,2=return SUCCESS/FAILURE; 3,4 exit SUCCESS/FAILURE\n");
    exit(EXIT_FAILURE);
  }

  if(atoi(argv[1])==1)
    return EXIT_SUCCESS;
  if(atoi(argv[1])==2)
    return EXIT_FAILURE;
  if(atoi(argv[1])==3)
    exit(EXIT_SUCCESS);
  if(atoi(argv[1])==4)
    exit(EXIT_FAILURE);
  return 1000;			/* not success or failure */
}

