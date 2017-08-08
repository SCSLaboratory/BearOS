#include <stdlib.h>
#include <utils/hash.h>

static hashtable_t *redirect_table;

struct redirect_entry {
  int filedes ;
  int filedes2;
};

typedef struct redirect_entry redirect_t;

static int redirect_init(void) {

  redirect_table = hopen(150); 
  /* 3 * 50 bc 50 is maxopen in piped, nfsd, bsock */
  if ( !redirect_table ) 
    return -1;

  return 0;
}

static int search_fn(void *elementp, const void *searchkeyp) {

  return ( ((redirect_t*)elementp)->filedes2 == *(int*)searchkeyp );
}

int dup2(int filedes, int filedes2) {

  redirect_t *entry;

  if ( !redirect_table ) {
    if ( redirect_init() < 0 )
      return -1;
  }

  if ( filedes == filedes2 ) 
    return -1;

  if ( filedes2 < 0 || filedes < 0 )
    return -1;
  
  /* we are supposed to close the file descriptor, but if we do that one 
     of the deamons could reassign it which we don't want... */
  //close(filedes2);

  entry = (redirect_t*)malloc(sizeof(redirect_t));
  if ( !entry ) 
    return -1;

  entry->filedes  = filedes ;
  entry->filedes2 = filedes2; 
  
  hput(redirect_table, (void*)entry, (char*)&(entry->filedes2), sizeof(int));
}

int apply_redirect(int filedes) {

  redirect_t *entry;

  if ( !redirect_table ) 
    return filedes;

  entry = (redirect_t*)hsearch(redirect_table, search_fn, 
			       (char*)&filedes, sizeof(int));

  return ( entry ? entry->filedes : filedes );
}
