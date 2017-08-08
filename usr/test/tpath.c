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
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <newlib.h>
#include <wordexp.h>

#define PATH_MAX 256

char *abspath(const char *file_name, char *resolved_name, char *pwd, char *home);

void testPath(const char *path,char *pwd,char *home) {
  char buff[PATH_MAX+1];
  printf("'%s' -> '%s'\n", path, abspath(path,buff,pwd,home));
}


int main() {
  char pwd[PATH_MAX];
  char home[PATH_MAX];
  strcpy(home,"/bear");
  strcpy(pwd,"/bear/x");
  testPath(".",pwd,home);
  testPath("..",pwd,home);
  testPath("abspath/",pwd,home);
  testPath("oops/",pwd,home);
  testPath("~/MyPath",pwd,home);
  testPath("../MyPath",pwd,home);
  testPath("foo/bar/../MyPath",pwd,home);
  testPath("../foo/bar/../MyPath",pwd,home);
  testPath("/foo/bar",pwd,home);
  testPath("/../foo",pwd,home);
  return 0;
}


static void appendCwd(const char *path, char *dst, char *pwd, char *home) {
  if (path[0]=='~') {
    strcpy(dst, home);
    strcat(dst, path+1);
  } 
  else if (path[0]=='/') {
    strcpy(dst, path);
  }
  else {
    strcpy(dst, pwd);
    strcat(dst, "/");
    strcat(dst, path);
    strcat(dst, "/");
  }
}

static void removeJunk(char *begin, char *end) {
  while(*end!=0) { 
    *begin++ = *end++; 
  }
  *begin = 0;
}

static char *manualPathFold(char *path) {
  char *s, *priorSlash;
  while ((s=strstr(path, "/../"))!=NULL) {
    *s = 0;
    if ((priorSlash = strrchr(path, '/'))==NULL) { /* oops */ 
      *s = '/'; 
      break; 
    }
    removeJunk(priorSlash, s+3);
  }
  while ((s=strstr(path, "/./"))!=NULL) { 
    removeJunk(s, s+2); 
  }
  while ((s=strstr(path, "//"))!=NULL) { 
    removeJunk(s, s+1); 
  }
  s = path + (strlen(path)-1);
  if (s!=path && *s=='/') { 
    *s=0; 
  }
  return path;
}

char *abspath(const char *file_name, char *resolved_name,char *pwd, char *home) {
  char buff[PATH_MAX+1];

  appendCwd(file_name,buff,pwd,home);
  strcpy(resolved_name, manualPathFold(buff)); 
  return resolved_name;
}
  
