#pragma once

#include "Define.h"

#include <iostream>
#include <mutex>
#include <queue>


/*----------------
*	ClientInfo
-----------------*/
class stClientInfo
{
public:
	stClientInfo()
	{
		memset(mRecvBuffer, 0, sizeof(stOverlappedEx));
		mSocket = INVALID_SOCKET;
	}

	void Init(const UINT32 _index)
	{
		mIndex = _index;
	}

	UINT32 GetIndex() { return mIndex; }
	bool IsConnected() { return mSocket != INVALID_SOCKET; }
	SOCKET	GetSock() { return mSocket; }
	char*	GetReceiveBuffer(){ return mRecvBuffer; }

	bool OnConnect(HANDLE _iocpHandle, SOCKET _socket)
	{
		mSocket = _socket;

		Clear();

		if (BindIOCompletionPort(_iocpHandle) == false)
		{
			return false;
		}

		return BindRecv();
	}

	void Close(bool bIsForce = true)
	{
		struct linger stLinger = { 0,0 };
		if (bIsForce == true)
		{
			stLinger.l_onoff = 1;
		}

		::shutdown(mSocket, SD_BOTH);

		::setsockopt(mSocket, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof(stLinger));

		::closesocket(mSocket);

		mSocket = INVALID_SOCKET;
	}

	void Clear() 
	{ 
	}

	bool BindIOCompletionPort(HANDLE _iocpHandle)
	{
		auto hIOCP = ::CreateIoCompletionPort((HANDLE)GetSock(), _iocpHandle, (ULONG_PTR)(this), 0);
		if (hIOCP == INVALID_HANDLE_VALUE)
		{
			std::cout << "SOCKET BIND ERROR : " << mIndex << " SOCKET ERROR, CODE : " << ::GetLastError() << std::endl;
			return false;
		}
		return true;
	}

	bool BindRecv()
	{
		DWORD dwFlag = 0;
		DWORD dwNumRecvBytes = 0;

		mRecvOverlappedEx.m_wsaBuf.len = MAX_SOCKBUF;
		mRecvOverlappedEx.m_wsaBuf.buf = mRecvBuffer;
		mRecvOverlappedEx.m_IOOperation = IOOperation::RECV;

		int nRet = WSARecv(mSocket,
			&(mRecvOverlappedEx.m_wsaBuf),
			1,
			&dwNumRecvBytes,
			&dwFlag,
			(LPWSAOVERLAPPED)&(mRecvOverlappedEx),
			NULL);

		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			std::cout << "WSA Recv () Failed, Code :" << WSAGetLastError() << std::endl;
			return false;
		}

		return true;
	}

	bool SendMsg(const UINT32 dataSize_, char* pMsg_)
	{
		auto SendOverlappedEx = new stOverlappedEx;
		memset(SendOverlappedEx, 0, sizeof(stOverlappedEx));
		SendOverlappedEx->m_wsaBuf.len = dataSize_;
		SendOverlappedEx->m_wsaBuf.buf = new char[dataSize_];
		CopyMemory(SendOverlappedEx->m_wsaBuf.buf, pMsg_, dataSize_);
		SendOverlappedEx->m_IOOperation = IOOperation::SEND;

		std::lock_guard<std::mutex> guard(mSendLock);

		mSendDataQueue.push(SendOverlappedEx);

		if (mSendDataQueue.size() == 1)
		{
			SendIO();
		}
		return true;
	}

	void SendCompleted(const UINT32 dataSize_)
	{
		std::cout << dataSize_ << ", Send Completed \n";

		std::lock_guard<std::mutex> guard(mSendLock);

		delete[] mSendDataQueue.front()->m_wsaBuf.buf;
		delete mSendDataQueue.front();

		mSendDataQueue.pop();

		if (mSendDataQueue.empty() == false) {
			SendIO();
		}
	}

private:
	bool SendIO()
	{
		auto sendOverlappedEx = mSendDataQueue.front();

		DWORD dwSendNumBytes = 0;
		int nRet = WSASend(mSocket,
			&(sendOverlappedEx->m_wsaBuf),
			1,
			&dwSendNumBytes,
			0,
			(LPWSAOVERLAPPED)sendOverlappedEx,
			NULL);

		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			std::cout << " WSASEND FALSE , CODE : " << WSAGetLastError() << std::endl;
			return false;
		}
		return true;
	}

	INT32							mIndex = 0;
	SOCKET							mSocket;					// client socket
	stOverlappedEx					mRecvOverlappedEx;		// for Receive OverlappedEx
	
	char							mRecvBuffer[MAX_SOCKBUF];

	std::mutex						mSendLock;
	std::queue<stOverlappedEx*>		mSendDataQueue;
};
