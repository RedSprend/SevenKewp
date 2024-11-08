#include "extdll.h"
#include "util.h"
#include "Socket.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h> 
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <string>
#include <stdint.h>

using namespace std;

struct SocketData
{
	int sock;
	sockaddr_in addr;
	sockaddr_in dest;
};

#ifndef SOMAXCONN
	#define SOMAXCONN 64
#endif

SocketData * Socket::createSocket(const char * addr, const char * port)
{
	SocketData * sock = new SocketData;
	memset(sock, 0, sizeof(SocketData));

	if (socketType & SOCKET_TCP) sock->sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	else						 sock->sock = socket(AF_INET, SOCK_DGRAM,  IPPROTO_UDP);

	if (sock->sock == -1)
	{
		ALERT(at_console, "Socket creation failed\n");
		delete (SocketData*)sock;
		return NULL;
	}

	if (!(socketType & SOCKET_BLOCKING))
	{
		int nFlags = fcntl(sock->sock, F_GETFL, 0);
		nFlags |= O_NONBLOCK;
		if (fcntl(sock->sock, F_SETFL, nFlags) == -1)
			ALERT(at_console, "Error setting socket to non-blocking\n");
	}

	return sock;
}

Socket::Socket()
{
	socketType = 0;
	sendBytes = recvBytes = 0;
	SocketData * sock = new SocketData;
	memset(sock, 0, sizeof(SocketData));
	skt = sock;
}

Socket::Socket(int socketType, ushort port)
{
	this->socketType = socketType | SOCKET_SERVER;
	sendBytes = recvBytes = 0;
	skt = createSocket(0,0);

	if (skt != NULL)
	{
		SocketData * sock = (SocketData*)skt;
		sock->addr.sin_family = AF_INET;
		sock->addr.sin_port = htons(port);
		sock->addr.sin_addr.s_addr = htonl(INADDR_ANY);
		if (bind()) {
			ALERT(at_console, "Created server socket on port %d\n", (int)port);
		}
		else {
			ALERT(at_console, "Failed to bind socket\n");
		}
	}
	
}

Socket::Socket(int socketType, IPV4 addr)
{
	this->socketType = socketType & ~SOCKET_SERVER;
	sendBytes = recvBytes = 0;
	skt = createSocket(0,0);

	if (skt != NULL)
	{
		SocketData * sock = (SocketData*)skt;
		sock->dest.sin_family = AF_INET;
		sock->dest.sin_port = htons(addr.port);
		string sAddr = addr.getHostString();
		int ret = inet_aton(sAddr.c_str(), &sock->dest.sin_addr);
		if (ret == 0)
		{
			ALERT(at_console, "inet_aton() failed\n");
			delete (SocketData*)sock;
			return;
		}
		ALERT(at_console, "Created client socket on %s\n", addr.getString().c_str());
	}
}

Socket::~Socket(void)
{
	SocketData * sock = (SocketData*)skt;
	if (sock != NULL)
	{
		close(sock->sock);
		delete (SocketData*)sock;
	}
}

bool Socket::connect(uint timeout)
{
	if (!(socketType & SOCKET_TCP))
	{
		ALERT(at_console, "Can't connect() with a UDP socket.\n");
		return false;
	}
	if (skt == NULL)
	{
		ALERT(at_console, "Can't connect. Invalid socket!\n");
		return false;
	}

	SocketData * sock = (SocketData*)skt;
	int ret = ::connect(sock->sock, (const sockaddr*)&sock->dest, sizeof(sock->dest));
	if (ret)
	{
		int err = errno;
		char buff[256];
		if (err == EALREADY || err == EINPROGRESS) // Would block
		{
			fd_set set;
			FD_ZERO(&set);
			FD_SET(sock->sock, &set);
			timeval tv;
			tv.tv_sec = (timeout / 1000);
			tv.tv_usec = (timeout % 1000)*1000;
			ret = select(sock->sock + 1, NULL, &set, NULL, &tv);

			if (ret == 0)
			{
				ALERT(at_console, "Socket connection timed out\n");
				return false;
			}
			else if (ret == -1)
			{
				ALERT(at_console, "Failed to connect socket: %s\n", strerror_r( err, buff, 256 ));
				return false;
			}
		}
		else
		{
			ALERT(at_console, "Failed to connect socket: %s\n", strerror_r( err, buff, 256 ));
			return false;
		}
	}

	return true;
}

Socket * Socket::accept()
{
	if (!(socketType & SOCKET_TCP))
	{
		ALERT(at_console, "Can't accept() with a UDP socket.\n");
		return NULL;
	}
	if (skt == NULL)
	{
		ALERT(at_console, "Can't accept. Invalid socket!\n");
		return NULL;
	}
	SocketData * sock = (SocketData*)skt;

	socklen_t len = sizeof(sockaddr_in);
	sockaddr_in newAddr;
	int newSock = ::accept(sock->sock, (sockaddr*)&newAddr, &len);
	if (newSock == -1)
	{
		int err = errno;
		if (err == EAGAIN) // Would block
			return NULL;

		char buff[256];
		ALERT(at_console, "Socket accept failed: %s\n", strerror_r( err, buff, 256 ));
		return NULL;
	}

	Socket * newClient = new Socket();
	newClient->socketType = this->socketType;
	SocketData* newData = (SocketData*)newClient->skt;
	newData->addr = sock->addr;
	newData->dest = newAddr;
	newData->sock = newSock;

	return newClient;
}

bool Socket::bind()
{
	SocketData * sock = (SocketData*)skt;
	int ret = ::bind(sock->sock, (sockaddr*) &sock->addr, sizeof(sock->addr));
	if (ret < 0)
	{
		ALERT(at_console, "Failed to bind socket\n");
		return false;
	}
	if (socketType & SOCKET_TCP)
	{
		ret = listen(sock->sock, SOMAXCONN);
		int err = errno;
		char buff[256];
		if (ret < 0)
		{
			ALERT(at_console, "Socket listen failed with error: %s\n", strerror_r( err, buff, 256 ));
			close(sock->sock);
			delete (SocketData*)skt;
			skt = NULL;
			return false;
		}

		fd_set set;
		FD_ZERO(&set);
		FD_SET(sock->sock, &set);

		int timeout = 1000;
		timeval tv;
		tv.tv_sec = (timeout / 1000);
		tv.tv_usec = (timeout % 1000)*1000;
		ret = select(sock->sock + 1, &set, NULL, NULL, &tv);

		err = errno;
		if (ret == 0)
		{
			ALERT(at_console, "Socket listen connection timed out\n");
			return false;
		}
		else if (ret == -1)
		{
			ALERT(at_console, "Failed to listen socket: %s\n", strerror_r( err, buff, 256 ));
			return false;
		}
	}
	return true;
}

bool Socket::recv(Packet& p)
{
	p.data = NULL;
	p.sz = -1;

	if (skt == NULL)
	{
		ALERT(at_console, "Can't recv. Invalid socket!\n");
		return false;
	}
	SocketData * sock = (SocketData*)skt;
	sockaddr_in addr;
	socklen_t slen = sizeof(addr);
	char dat[MAX_PACKET_SIZE];
	int len;

	if (socketType & SOCKET_TCP) 
		len = ::recv(sock->sock, dat, MAX_PACKET_SIZE, 0);
	else                         
		len = recvfrom(sock->sock, dat, MAX_PACKET_SIZE, 0, (sockaddr*)&addr, &slen);

	if (len == 0)
		return false;
	if (len == -1)
	{
		int err = errno;
		if ((err == EAGAIN) || (err == EWOULDBLOCK))
			return false;
		char buff[256];
		ALERT(at_console, "Socket recv failed with error: %s\n", strerror_r( err, buff, 256 ));
		return false;
	}

	uint a = addr.sin_addr.s_addr;
	uint8_t b4 = (a >> 24) & 0xff;
	uint8_t b3 = (a >> 16) & 0xff;
	uint8_t b2 = (a >> 8) & 0xff;
	uint8_t b1 = a & 0xff;
	IPV4 sender(b1, b2, b3, b4, ntohs(addr.sin_port));

	if (p.data != NULL)
		delete [] p.data;
	p.addr = sender;
	p.data = new char[len];
	memcpy(p.data, dat, len);
	p.sz = len;
	recvBytes += len + (socketType & SOCKET_TCP) ? IPV4_TCP_HEADER_SIZE : IPV4_UDP_HEADER_SIZE;
	return true;
}

bool Socket::send( const Packet& p )
{
	if (skt == NULL)
	{
		ALERT(at_console, "Can't send. Invalid socket!\n");
		return false;
	}
	if (p.sz > MAX_PACKET_SIZE)
	{
		ALERT(at_console, "Packet too big to send! (%d > %d)\n", p.sz, MAX_PACKET_SIZE);
		return false;
	}

	SocketData * sock = (SocketData*)skt;
	sockaddr_in addr;
	if (!p.addr.isEmpty())
	{
		string saddr = p.addr.getHostString();
		memset(&addr.sin_zero, 0, sizeof(addr.sin_zero));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = inet_addr( saddr.c_str() );
		addr.sin_port = htons(p.addr.port);
	}
	else
		addr = sock->dest;

	int ret;
	if (socketType & SOCKET_TCP) ret = ::send(sock->sock, p.data, p.sz, MSG_NOSIGNAL);
	else						 ret = sendto(sock->sock, p.data, p.sz, 0, (sockaddr*)&addr, sizeof(addr));

	char buff[256];
	if ( ret < 0 )
	{
		int err = errno;
		if ((err == EAGAIN) || (err == EWOULDBLOCK)) {
			ALERT(at_console, "Send would block so don't send\n");
			return false;
		}
		ALERT(at_console, "Socket send failed with error: %s\n", strerror_r( err, buff, 256 ));
		return false;
	}

	sendBytes += ret + (socketType & SOCKET_TCP) ? IPV4_TCP_HEADER_SIZE : IPV4_UDP_HEADER_SIZE;
	return true;
}

bool Socket::send( const void * dat, int sz )
{
	return send(Packet(dat, sz));
}
