#pragma once

#include "Define.h"
#include <string>
#include <mutex>
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


/*----------------
*	Client Info 
-----------------*/
class stClientInfo
{
public:
	stClientInfo()
	{
		ZeroMemory(&mSendOverlappedEx, sizeof(stOverlappedEx));
		ZeroMemory(&mRecvOverlappedEx, sizeof(stOverlappedEx));
		m_sock = INVALID_SOCKET;
	}

	~stClientInfo() = default;

	void Init(const UINT32 _index)
	{
		m_Index = _index;
	}

	UINT32 GetIndex() const { return m_Index; }

	bool IsConnected() { return m_sock != INVALID_SOCKET; }

	SOCKET	GetSock() { return m_sock; }

	char* RecvBuffer() { return mSendBuf; }

	// connect client socket and iocpHandle
	bool OnConnect(HANDLE IocpHandle_, SOCKET socket)
	{
		m_sock = socket;

		Clear();

		if (BindIOCompletionPort(IocpHandle_) == false)
		{
			return false;
		}

		return BindRecv();
	}

	void Close(bool bIsForce = false)
	{
		struct linger stLinger = { 0, 0 };

		if (true == bIsForce)
		{
			stLinger.l_onoff = 1;
		}

		shutdown(m_sock, SD_BOTH);

		setsockopt(m_sock, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof(stLinger));

		closesocket(m_sock);
		m_sock = INVALID_SOCKET;
	}

	void Clear()
	{
		mSendPos = 0;
		mIsSending = false;
	}

	bool BindIOCompletionPort(HANDLE IocpHandle_)
	{
		auto hIOCP = CreateIoCompletionPort((HANDLE)GetSock(), IocpHandle_, (ULONG_PTR)this, 0);
		if (hIOCP == INVALID_HANDLE_VALUE)
		{
			GetLastErrorAsString();
			return false;
		}

		return true;
	}

	bool BindRecv()
	{
		DWORD	dwFlag = 0;
		DWORD	dwRecvNumBytes = 0;

		mRecvOverlappedEx.m_wsaBuf.len = MAX_SOCKBUF;
		mRecvOverlappedEx.m_wsaBuf.buf = mRecvBuf;
		mRecvOverlappedEx.m_Operation = IOOperation::RECV;

		int nRet = WSARecv(m_sock,
						&(mRecvOverlappedEx.m_wsaBuf),
						1,
						&dwRecvNumBytes,
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

	bool SendMsg(const UINT32 dataSize_, char* pMsg)
	{
		std::lock_guard<std::mutex> guard(mSendLock);
		// TODO: will be emplaced to circular queue
		if ((mSendPos + dataSize_) > MAX_SOCKBUF)
		{
			mSendPos = 0;
		}

		auto pSendBuf = &mSendBuf[mSendPos];

		// copy data to Buf
		CopyMemory(pSendBuf, pMsg, dataSize_);
		mSendPos += dataSize_;
			 
		return true;
	}

	bool SendIO()
	{
		if (mSendPos < 0 || mIsSending) { return true; }

		std::lock_guard<std::mutex> guard(mSendLock);

		mIsSending = true;

		CopyMemory(mSendingBuf, &mSendBuf[0], mSendPos);

		mIsSending = true;

		mSendOverlappedEx.m_wsaBuf.len = mSendPos;
		mSendOverlappedEx.m_wsaBuf.buf = &mSendingBuf[0];
		mSendOverlappedEx.m_Operation = IOOperation::SEND;

		DWORD dwnumOfBytes = 0;
		int nRet = WSASend(m_sock,
						&(mSendOverlappedEx.m_wsaBuf),
						1,
						&dwnumOfBytes,
						0,
						(LPWSAOVERLAPPED)&(mSendOverlappedEx),
						NULL);

		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			GetLastErrorAsString();
			return false;
		}
		
		// the sendIO finished so, set mSendPos to 0
		mSendPos = 0;

		return true;
	}

	void SendCompleted(const UINT32 dataSize_)
	{
		mIsSending = true;
		std::cout << " Send Completed : " << dataSize_ << " size \n";
	}

private:
	UINT32				m_Index = 0;
	SOCKET				m_sock = INVALID_SOCKET;
	stOverlappedEx		mRecvOverlappedEx; // overlappedEx for Recv
	stOverlappedEx		mSendOverlappedEx; // overlappedEx for Send

	char				mRecvBuf[MAX_SOCKBUF]; // data buffer

	std::mutex			mSendLock;
	bool				mIsSending = false;
	UINT64				mSendPos = 0;
	char				mSendBuf[MAX_SOCK_SENDBUF]; // data buffer
	char				mSendingBuf[MAX_SOCK_SENDBUF]; // data buffer
};