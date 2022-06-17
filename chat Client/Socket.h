#pragma once

#include <WinSock2.h>
#include <Windows.h>
#include <mswsock.h>
#include <string>


class Endpoint;  // Util.h

enum class SocketType
{
	Tcp,
	Udp,
};

class Socket
{
public:

	static const int MaxReceiveLength = 4096;
	SOCKET m_fd;

	LPFN_ACCEPTEX AcceptEx = NULL;
	bool m_isReadOverlapped = false;
	WSAOVERLAPPED m_readOverlappedStruct;

	char m_receiveBuffer[MaxReceiveLength]; // RECV BUF
	DWORD m_readFlags = 0;

	Socket();
	Socket(SOCKET fd);
	Socket(SocketType socketType);
	~Socket();

	bool Init();
	void Bind(const Endpoint& endpoint);
	void Connect(const Endpoint& endpoint);
	int Send(const char* data, int length);
	void Close();
	void Listen();
	int Accept(Socket& acceptedSocket, std::string& errorText);
	int Receive();
	void Print();
};