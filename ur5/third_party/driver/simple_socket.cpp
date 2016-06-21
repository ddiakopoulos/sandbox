#include "simple_socket.hpp"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#ifdef WIN32
void bzero(void *s, size_t n)
{
    memset(s, 0, n);
}
#endif

#ifndef WIN32
void closesocket(int fd)
{
    close(fd);
}
#endif

int internal_socket(int domain, int type, int protocol)
{
    int val;
    int s = socket(domain, type, protocol);
    if (s < 0) return s;
    val = 4096;
    setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char*)&val, sizeof(int));
    val = 4096;
    setsockopt(s, SOL_SOCKET, SO_SNDBUF, (char*)&val, sizeof(int));
    return s;
}

bool set_nonblock(int socket, int blocking)
{
#if defined(WIN32)
	unsigned long flags = blocking ? 0 : 1;
	return (ioctlsocket(socket, FIONBIO, &flags) == 0) ? true : false;
#else
	int flags = fcntl(sockfd, F_GETFL, 0);
	if (flags < 0)
	{
		return false;
	}
	flags = blocking ? (flags&~O_NONBLOCK) : (flags | O_NONBLOCK);
    return (fcntl(socket, F_SETFL, flags) == 0) ? true : false;
#endif
}

// Returns a socket on which to accept() connections
int open_tcp_port(short portno)
{
    int sockfd;
    struct sockaddr_in serv_addr;

    sockfd = internal_socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0) return -1;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(portno);
    if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) 
    {
        return -1;
    }

    listen(sockfd, 5);
    return sockfd;
}

// Returns a socket to use to send data to the given address
int connect_to_host(const char * hostname, short portno)
{
    struct hostent * host_info;
    int sockfd, yes = 1;
    struct sockaddr_in host_addr;

    host_info = gethostbyname(hostname);
    if (host_info == NULL) 
    {
        return -1;
    }

    sockfd = internal_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0) 
    {
        return -1;
    }

    (void) setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(int));
    (void) setsockopt(sockfd,IPPROTO_TCP, TCP_NODELAY, (char*)&yes, sizeof(int));

    memset((char*)&host_addr, 0, sizeof(host_addr));
    host_addr.sin_family = host_info->h_addrtype;

    memcpy((char*)&host_addr.sin_addr, host_info->h_addr,  host_info->h_length);

    host_addr.sin_port = htons(portno);

    if (connect(sockfd, (struct sockaddr*)&host_addr, sizeof(struct sockaddr_in)) != 0) 
    {
        closesocket(sockfd);
        return -1;
    }

    return sockfd;
}

int connect_to_addr(struct sockaddr_in * host_addr)
{
    int sockfd, yes = 1;

    sockfd = internal_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0) 
    {
        return -1;
    }

    (void) setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(int));

    if (connect(sockfd, (struct sockaddr*)host_addr, sizeof(struct sockaddr_in)) != 0) 
    {
        closesocket(sockfd);
        return -1;
    }

    return sockfd;
}

// repeated send until all of buffer is sent
int sendall(int sockfd, char * buffer, int bufLen, int flags)
{
    int numBytesToSend = bufLen, length;

    while (numBytesToSend>0) 
    {
        length = send(sockfd, buffer, numBytesToSend, flags);
        if (length < 0) 
        {
            return(-1);
        }
        numBytesToSend -= length;
        buffer += length ;
    }
    return(bufLen);
}

// repeated read until all of buffer is read 
int readall(int sockfd, char * buffer, int bufLen, int flags)
{
    int numBytesToRead = bufLen, length;

    while (numBytesToRead > 0) 
    {
        length = recv(sockfd, buffer, numBytesToRead, flags);
        if (length <= 0) 
        {
            return(length);
        }
        numBytesToRead -= length;
        buffer += length;
    }
    return (bufLen);
}

int addr_eq(struct sockaddr_in * a, struct sockaddr_in * b)
{
    if (a->sin_port == b->sin_port && a->sin_addr.s_addr == b->sin_addr.s_addr) return 1;
    return 0;
}

int socket_ready(int sock, int t)
{
    fd_set fds;
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = t;

    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    select(sock + 1, &fds, NULL, NULL, &timeout);
    return FD_ISSET(sock, &fds);
}
