#pragma once

#pragma comment(lib, "ws2_32")

#include <winsock2.h>
#include <Ws2tcpip.h>


const UINT32 MAX_SOCKBUF = 256;
const UINT32 MAX_WORKERTHREAD = 4;

enum class IOOperation
{
	RECV,
	SEND
};

struct stOverlappedEx
{
	WSAOVERLAPPED	wsaOverlapped;			// Overlapped IO ±¸Á¶Ã¼
	SOCKET			m_socket;				// client socket
	WSABUF			m_wsaBuf;				// Overlapped IO struct
	char			m_szBuf[MAX_SOCKBUF];	// data buffer
	IOOperation		m_Operation;			// type of operation
};

struct stClientInfo
{
	INT32				mIndex = 0;
	SOCKET				m_socketClient;
	stOverlappedEx		m_stRecvOverlappedEx;
	stOverlappedEx		m_stSendOverlappedEx;

	char				mRecvBuf[MAX_SOCKBUF];
	char				mSendBuf[MAX_SOCKBUF];

	stClientInfo() {
		m_socketClient = INVALID_SOCKET;
		ZeroMemory(&m_stRecvOverlappedEx, sizeof(m_stRecvOverlappedEx));
		ZeroMemory(&m_stSendOverlappedEx, sizeof(m_stSendOverlappedEx));
	}
};
