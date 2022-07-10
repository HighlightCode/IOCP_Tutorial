#pragma once

#pragma comment(lib,"ws2_32")

#include <WinSock2.h>
#include <WS2tcpip.h>

const UINT32 MAX_SOCKBUF = 256;	// MAX SOCKET BUFFER
const UINT32 MAX_SOCKSENDBUF = 4096; // MAX SOCKET SEND BUFFER
const UINT32 MAX_WORKERTHREAD = 4;  // MAX WORKER THREAD

/*-------------------
*	  IOOperation
--------------------*/
enum class IOOperation
{
	RECV,
	SEND
};

/*---------------------
*	stOverlappedEx
----------------------*/
struct stOverlappedEx
{
	WSAOVERLAPPED		m_wsaOverlapped;
	SOCKET				m_socketClient;
	WSABUF				m_wsaBuf;
	IOOperation			m_IOOperation;
};