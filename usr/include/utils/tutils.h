#pragma once

#include <utils/bool.h>

#define NAMESIZE 256

typedef struct person_struct {
  char person_name[NAMESIZE];
  int person_age;
  double *bigarray;   
  double person_checksum;
} person_t;

#define bigarrayval(pp,i) (*((pp->bigarray)+i))

void* make_person(char* namep,int age,int arraysize);
void free_person(void *pp);
int check_person(void *pp,char *namep,int age,int arraysize);
void print_person(void *pp);
void dotme(int n,int i);
int is_age(void *ep,const void *keyp);
int start_daemon(char *cmdp);
