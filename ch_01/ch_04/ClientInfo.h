#pragma once

#include "Define.h"
#include <stdio.h>
#include <string>
#include <iostream>

void GetLastErrorAsString()
{
	//Get the error message ID, if any.
	DWORD errorMessageID = ::GetLastError();

	LPSTR messageBuffer = nullptr;

	size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

	//Copy the error message into a std::string.
	std::string message(messageBuffer, size);
	std::cout << message << std::endl;

	//Free the Win32's string's buffer.
	LocalFree(messageBuffer);
}


// struct for Client 
class stClientInfo
{
public:
	stClientInfo()
	{
		ZeroMemory(&mRecvOverlappedEx, sizeof(stOverlappedEx));
		mSock = INVALID_SOCKET;
	}

	void Init(const UINT32 index)
	{
		m_index = index;
	}

	UINT32	GetIndex() const { return m_index; }

	bool	IsConnected() const { return mSock != INVALID_SOCKET; }

	SOCKET	GetSock()	const { return mSock; }

	char*	RecvBuffer() { return mRecvBuf; }

	bool OnConnect(HANDLE IocpHandle, SOCKET sock_)
	{
		mSock = sock_;

		Clear();

		if (BindIOCompletionPort(IocpHandle) == false)
		{
			return false;
		}

		return true;
	}

	void Clear()
	{
	}

	void Close(bool bIsForce = false)
	{
		struct linger stLinger = { 0, 0 };	// SO_DONTLINGER

		if (true == bIsForce)
		{
			stLinger.l_onoff = 1;
		}

		shutdown(mSock, SD_BOTH);

		// set sock option to Linger
		setsockopt(mSock, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof(stLinger));

		// close socket
		closesocket(mSock);

		mSock = INVALID_SOCKET;
	}

	bool BindIOCompletionPort(HANDLE IocpHandle_)
	{
		// bind between Socket and iocpHandle
		auto hIocp = CreateIoCompletionPort((HANDLE)GetSock(), IocpHandle_, (ULONG_PTR)(this), 0);
		
		if (hIocp == INVALID_HANDLE_VALUE)
		{
			return false;
		}

		return true;
	}

	bool BindRecv()
	{
		DWORD	dwFlag = 0;
		DWORD	dwNumOfBytes = 0;

		mRecvOverlappedEx.m_wsaBuf.len = MAX_SOCKBUF;
		mRecvOverlappedEx.m_wsaBuf.buf = mRecvBuf;
		mRecvOverlappedEx.m_Operation = IOOperation::RECV;

		int nRet = WSARecv(mSock,
						&(mRecvOverlappedEx.m_wsaBuf),
						1,
						&dwNumOfBytes,
						&dwFlag,
						(LPWSAOVERLAPPED)&(mRecvOverlappedEx),
						NULL);

		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			GetLastErrorAsString();
			return false;
		}

		return true;
	}

	bool SendMsg(const UINT32 dataSize_, char* pMsg_)
	{
		auto sendOverlappedEx = new stOverlappedEx;
		ZeroMemory(sendOverlappedEx, sizeof(stOverlappedEx));
		sendOverlappedEx->m_wsaBuf.len = dataSize_;
		sendOverlappedEx->m_wsaBuf.buf = new char[dataSize_];
		CopyMemory(sendOverlappedEx->m_wsaBuf.buf, pMsg_, dataSize_);
		sendOverlappedEx->m_Operation = IOOperation::SEND;

		DWORD dwNumOfBytes = 0;

		int nRet = WSASend(mSock,
			&(sendOverlappedEx->m_wsaBuf),
			1,
			&dwNumOfBytes,
			0,
			(LPWSAOVERLAPPED)sendOverlappedEx,
			NULL);

		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			GetLastErrorAsString();
			return false;
		}

		return true;
	}

	void SendComplete(const UINT32 datasize)
	{
		std::cout << "data send complete, size: " << datasize << std::endl;
	}

private:
	UINT32			m_index = 0;
	SOCKET			mSock;					// socket for connected client
	stOverlappedEx	mRecvOverlappedEx;		// for Recv Overlapped IO
	char			mRecvBuf[MAX_SOCKBUF];	// data buf
};