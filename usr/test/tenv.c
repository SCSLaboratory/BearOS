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
 * tenv -- tests command line arguments 
 */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <newlib.h>

#define OVERWRITE 1

/* typical values provided by a shell */
#define PATHSTR "/bin:.:./bin"
#define HOMESTR "/home/bear"
#define LOGSTR "bear"
#define PWDSTR "/home/bear"
/* testing values */
#define NEWPATHSTR "/foo:/bar"
#define NEWHOMESTR "/fudge/foo"
#define NEWLOGSTR "foo"
#define NEWPWDSTR "/fudge/foo"

#define TENV_DEBUG 1

int main(int argc, char *argv[]) {

  if(argc!=1)  {
    printf("[Usage: tenv]\n");
    exit(EXIT_FAILURE);
  }

  /* grab the env */
  if(strcmp(getenv("HOME"),HOMESTR)!=0) {
#ifdef TENV_DEBUG
    printf("[ERROR: home expected %s, found %s]\n",getenv("HOME"),HOMESTR);
#endif
    exit(EXIT_FAILURE);
  }
  if(strcmp(getenv("PATH"),PATHSTR)!=0) {
#ifdef TENV_DEBUG
    printf("[ERROR: path expected %s, found %s]\n",getenv("PATH"),PATHSTR);
#endif
    exit(EXIT_FAILURE);
  }
  if(strcmp(getenv("LOGNAME"),LOGSTR)!=0) {
#ifdef TENV_DEBUG
    printf("[ERROR: logname expected %s, found %s]\n",getenv("LOGNAME"),LOGSTR);
#endif
    exit(EXIT_FAILURE);
  }
  if(strcmp(getenv("PWD"),PWDSTR)!=0) {
#ifdef TENV_DEBUG
    printf("[ERROR: pwd expected %s, found %s]\n",getenv("PWD"),PWDSTR);
#endif
    exit(EXIT_FAILURE);
  }

  /* set it differently */
  setenv("HOME",NEWHOMESTR,OVERWRITE);
  setenv("PATH",NEWPATHSTR,OVERWRITE);
  setenv("LOGNAME",NEWLOGSTR,OVERWRITE);
  setenv("PWD",NEWPWDSTR,OVERWRITE);
  /* check its different */
  if(strcmp(getenv("HOME"),NEWHOMESTR)!=0) {
#ifdef TENV_DEBUG
    printf("[ERROR: new home expected %s, found %s]\n",getenv("HOME"),NEWHOMESTR);
#endif
    exit(EXIT_FAILURE);
  }
  if(strcmp(getenv("PATH"),NEWPATHSTR)!=0) {
#ifdef TENV_DEBUG
    printf("[ERROR: new path expected %s, found %s]\n",getenv("PATH"),NEWPATHSTR);
#endif
    exit(EXIT_FAILURE);
  }
  if(strcmp(getenv("LOGNAME"),NEWLOGSTR)!=0) {
#ifdef TENV_DEBUG
    printf("[ERROR: new logname expected %s, found %s]\n",getenv("LOGNAME"),NEWLOGSTR);
#endif
    exit(EXIT_FAILURE);
  }
  if(strcmp(getenv("PWD"),NEWPWDSTR)!=0) {
#ifdef TENV_DEBUG
    printf("[ERROR: new pwd expected %s, found %s]\n",getenv("PWD"),NEWPWDSTR);
#endif
    exit(EXIT_FAILURE);
  }

  /* set it back */
  setenv("HOME",HOMESTR,OVERWRITE);
  setenv("PATH",PATHSTR,OVERWRITE);
  setenv("LOGNAME",LOGSTR,OVERWRITE);
  setenv("PWD",PWDSTR,OVERWRITE);

  /* check its back */
  if(strcmp(getenv("HOME"),HOMESTR)!=0) {
#ifdef TENV_DEBUG
    printf("[ERROR: reset home expected %s, found %s]\n",getenv("HOME"),HOMESTR);
#endif
    exit(EXIT_FAILURE);
  }
  if(strcmp(getenv("PATH"),PATHSTR)!=0) {
#ifdef TENV_DEBUG
    printf("[ERROR: reset path expected %s, found %s]\n",getenv("PATH"),PATHSTR);
#endif
    exit(EXIT_FAILURE);
  }
  if(strcmp(getenv("LOGNAME"),LOGSTR)!=0) {
#ifdef TENV_DEBUG
    printf("[ERROR: reset logname expected %s, found %s]\n",getenv("LOGNAME"),LOGSTR);
#endif
    exit(EXIT_FAILURE);
  }
  if(strcmp(getenv("PWD"),PWDSTR)!=0) {
#ifdef TENV_DEBUG
    printf("[ERROR: reset pwd expected %s, found %s]\n",getenv("PWD"),PWDSTR);
#endif
    exit(EXIT_FAILURE);
  }

  /* yippee it worked */
  return EXIT_SUCCESS;
}




