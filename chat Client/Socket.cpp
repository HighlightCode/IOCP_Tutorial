#include "Socket.h"
#include <iostream>
#include <string>
#include <sstream>
#include "Util.h"

Socket::Socket()
{
	static_assert(-1 == INVALID_SOCKET, "");
	m_fd = -1;
	ZeroMemory(&m_readOverlappedStruct, sizeof(m_readOverlappedStruct));
}

Socket::Socket(SOCKET fd)
{
	Init();
	m_fd = fd;
	ZeroMemory(&m_readOverlappedStruct, sizeof(m_readOverlappedStruct));
}

Socket::Socket(SocketType socketType)
{
	Init();
	if (socketType == SocketType::Tcp)
	{
		m_fd = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	}
	else
	{
		m_fd = WSASocket(AF_INET, SOCK_DGRAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	}
}

Socket::~Socket()
{
	Close();
}

bool Socket::Init()
{
	WSADATA wsaData;
	int err = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (err != 0) {
		/* Can't startup WINSOCK DLL*/
		std::cout << "Can't initialize winsock : " << err << std::endl;
		return false;
	}
}

void Socket::Bind(const Endpoint & endpoint)
{
	if (bind(m_fd, (sockaddr*)&endpoint.m_ipv4Endpoint, sizeof(endpoint.m_ipv4Endpoint)) < 0)
	{
		std::stringstream ss;
		ss << "bind failed:";
		throw std::exception(ss.str().c_str());
	}
}

void Socket::Connect(const Endpoint & endpoint)
{
	if (connect(m_fd, (sockaddr*)&endpoint.m_ipv4Endpoint, sizeof(endpoint.m_ipv4Endpoint)) < 0)
	{
		std::stringstream ss;
		ss << "connect failed:";
		throw std::exception(ss.str().c_str());
	}
}

int Socket::Send(const char * data, int length)
{
	return ::send(m_fd, data, length, 0);
}

void Socket::Close()
{
	closesocket(m_fd);
}

void Socket::Listen()
{
	listen(m_fd, 11021);
}

int Socket::Accept(Socket & acceptedSocket, std::string & errorText)
{
	acceptedSocket.m_fd = accept(m_fd, NULL, 0);
	if (acceptedSocket.m_fd == -1)
	{
		return -1;
	}
	else
		return 0;
}

int Socket::Receive()
{
	return (int)recv(m_fd, m_receiveBuffer, MaxReceiveLength, 0);
}

void Socket::Print()
{
	std::cout << m_receiveBuffer << std::endl;
}
