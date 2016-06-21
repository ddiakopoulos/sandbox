#pragma once

#ifndef simple_socket_h
#define simple_socket_h

#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef WIN32
#include <winsock2.h>
#pragma comment(lib, "wsock32.lib")
#else
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#endif

#ifdef WIN32
void bzero(void * s, size_t n);
#endif

#ifndef WIN32
void closesocket(int fd);
#endif

int open_tcp_port(short portno);
int connect_to_host(const char * hostname, short portno);
int connect_to_addr(struct sockaddr_in * host_addr);
int sendall(int sockfd, char * buffer, int bufLen, int flags);
int readall(int sockfd, char * buffer, int bufLen, int flags);
bool set_nonblock(int socket, int blocking);
int addr_eq(struct sockaddr_in * a, struct sockaddr_in * b);
int socket_ready(int sock, int timeout);

#endif
