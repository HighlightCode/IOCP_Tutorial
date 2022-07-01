#pragma once

#pragma comment(lib,"ws2_32")

#include <WinSock2.h>
#include <WS2tcpip.h>

const UINT32 MAX_SOCKBUF = 32; // size of Socket Buffer
const UINT32 MAX_SOCK_SENDBUF = 4096; // size of Socket Buffer
const UINT32 MAX_WORKERTHRED = 4; // number of worker Thread

enum class IOOperation
{
	RECV,
	SEND
};

// expand Structure of wsaOverlapped struct
struct stOverlappedEx
{
	WSAOVERLAPPED		m_wsaOverlapped;		// struct of Overlapped IO
	SOCKET				m_socketClient;			// socket 
	WSABUF				m_wsaBuf;				// Overlapped IO buffers
	IOOperation			m_Operation;
};
