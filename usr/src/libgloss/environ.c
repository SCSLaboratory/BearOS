#include <stdint.h>
#include <unistd.h>
/*
 * environ variable
 */

char **environ;

void env_init() {
  uint64_t rcx;
  asm volatile("movq %%rcx,%0" : "=r"(rcx));
	
  environ = (char**)rcx;	
	return;
}
