#pragma once
/*
 * netdb.h -- BSD sockets translated to use the bear network daemon
 */

#ifdef KERNEL
#include <stdint.h>
#else
#include <stddef.h> /* for size_t */
#endif

#include <arpa/inet.h>
#include <netinet/in.h>

struct hostent {
    char  *h_name;      /* Official name of the host. */
    char **h_aliases;   /* A pointer to an array of pointers to alternative host names,
                           terminated by a null pointer. */
    int    h_addrtype;  /* Address type. */
    int    h_length;    /* The length, in bytes, of the address. */
    char **h_addr_list; /* A pointer to an array of pointers to network addresses (in
                           network byte order) for the host, terminated by a null pointer. */
#define h_addr h_addr_list[0] /* for backward compatibility */
};

struct addrinfo {
    int               ai_flags;      /* Input flags. */
    int               ai_family;     /* Address family of socket. */
    int               ai_socktype;   /* Socket type. */
    int               ai_protocol;   /* Protocol of socket. */
    socklen_t         ai_addrlen;    /* Length of socket address. */
    struct sockaddr  *ai_addr;       /* Socket address of socket. */
    char             *ai_canonname;  /* Canonical name of service location. */
    struct addrinfo  *ai_next;       /* Pointer to next in list. */
};

struct servent { 
  char *s_name; 		/* service name */
  char **s_aliases;		/* alias list */
  int s_port;			/* port number */
  char *s_proto;		/* protocol to use */
};

struct hostent *gethostbyname(const char *name);
struct hostent *gethostbyaddr(const void *addr, socklen_t len, int type);
struct servent *getservbyname(const char *name, const char *proto);
