#pragma once
#pragma comment(lib,"ws2_32")

#include <WS2tcpip.h>
#include <WinSock2.h>
#include <string>

class Endpoint
{
public:
	Endpoint();
	Endpoint(const char* address, int port);
	~Endpoint();

	sockaddr_in m_ipv4Endpoint;

	static Endpoint Any;
	std::string ToString();
};