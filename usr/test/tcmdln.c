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
#include <string.h>

/*
 * tcmdln -- tests command line arguments, invoke with:
 *    tcmdln 123
 *    tcmdln 123 foobar
 *    tcmdln 123 foobar 3.142
 * all other inputs should fail
 */
#define BUFFLEN 256

int main(int argc, char *argv[]) {
  int arg1;
  double arg3;

  switch(argc) {
  case 1:
    if(strcmp(argv[0],"tcmdln")!=0)
      return EXIT_FAILURE;
    break;
  case 2:
    if(strcmp(argv[0],"tcmdln")!=0)
      return EXIT_FAILURE;
    if((arg1 = atoi(argv[1]))!=123)
      return EXIT_FAILURE;
    break;
  case 3:
    if(strcmp(argv[0],"tcmdln")!=0)
      return EXIT_FAILURE;
    if((arg1 = atoi(argv[1]))!=123)
      return EXIT_FAILURE;
    if(strcmp(argv[2],"foobar")!=0)
      return EXIT_FAILURE;
    break;
  case 4:
    if(strcmp(argv[0],"tcmdln")!=0)
      return EXIT_FAILURE;
    if((arg1 = atoi(argv[1]))!=123)
      return EXIT_FAILURE;
    if(strcmp(argv[2],"foobar")!=0)
      return EXIT_FAILURE;
    if((arg3 = atof(argv[3]))!=3.142)
      return EXIT_FAILURE;
    break;
  default:
    return EXIT_FAILURE;
    break;
  }
  return EXIT_SUCCESS;
}
