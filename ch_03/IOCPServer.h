#pragma once

#pragma comment(lib, "ws2_32")

#include "Define.h"
#include <thread>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <string>
#include <iostream>
#include <vector>


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

class IOCPServer
{
public:
	IOCPServer(void) {};

	~IOCPServer(void)
	{
		WSACleanup(); // end up winsock
	}

	bool InitSocket()
	{
		WSADATA wsaData;
		int err = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (err != 0) {
			/* Can't startup WINSOCK DLL*/
			std::cout << "Can't initialize winsock : " << err << std::endl;
			return false;
		}

		mListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);

		if (mListenSocket == INVALID_SOCKET) {
			/* socket error */
			std::cout << "error socket() function return false" << std::endl;
			return false;
		}

		return true;
	}

	bool bindAndListen(int nBindPort)
	{
		SOCKADDR_IN			stServAddr;
		stServAddr.sin_family = AF_INET;
		stServAddr.sin_port = htons(nBindPort);
		stServAddr.sin_addr.s_addr = htonl(INADDR_ANY);

		int err = bind(mListenSocket, (SOCKADDR*)&stServAddr, sizeof(SOCKADDR_IN));
		if (err != 0) {
			std::cout << "bind error occured : " << err << std::endl;
			return false;
		}

		// socket listen
		err = listen(mListenSocket, 10);
		if (err != 0) {
			std::cout << "listen error occured : " << err << std::endl;
			return false;
		}

		std::cout << "socket bind successfully : " << nBindPort << std::endl;
		return true;
	}

	bool StartServer(const UINT32 maxClientCount)
	{
		CreateClient(maxClientCount);
		mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, MAX_WORKERTHREAD);
		if (mIOCPHandle == NULL) {
			GetLastErrorAsString();
		}

		//Create Worker Thread
		bool bret = CreateWorkerThread();
		if (bret == false)
		{
			return false;
		}

		// Create Accept Thread
		bret = CreateAcceptThread();
		if (bret == false)
		{
			return false;
		}

		return true;
	}

	// Destroy the working thread;
	void DestroyThread()
	{
		m_isWorkerRun = false;
		CloseHandle(mIOCPHandle);

		for (auto& th : mIOWorkerThread)
		{
			if (th.joinable())
				th.join();
		}

		// close accept thread
		m_isAcceptRun = false;
		closesocket(mListenSocket);

		if (mAcceptThread.joinable())
		{
			mAcceptThread.join();
		}
	}

	virtual void OnConnect(const UINT32 clientIndex_) {}	
	virtual void OnClose(const UINT32 clientIndex_) {}
	virtual void OnReceive(const UINT32 clientIndex_, const UINT32 size_, char* pData_) {}


private:
	
	void CreateClient(const UINT32 maxClientCnt)
	{
		for (UINT32 i = 0; i < maxClientCnt; i++) {
			mClientInfos.emplace_back();
			mClientInfos[i].mIndex = i;
		}
	}

	bool CreateWorkerThread()
	{
		unsigned int uiThreadId = 0;
		for (int i = 0; i < MAX_WORKERTHREAD; i++) {
			mIOWorkerThread.emplace_back([this]() {WorkerThread(); });
		}
		std::cout << "Worker Thread Init" << std::endl;
		return true;
	}

	bool CreateAcceptThread()
	{
		mAcceptThread = std::thread([this]() {AcceptThread(); });
		std::cout << "Accept Thread Init " << std::endl;
		return true;
	}

	stClientInfo* GetEmptyClientInfo()
	{
		for (auto& c : mClientInfos) 
		{
			if (c.m_socketClient == INVALID_SOCKET) 
			{
				return &c;
			}
		}
		return nullptr;
	}

	bool bindIOCompletionPort(stClientInfo* pClientInfo)
	{
		auto hIOCP = CreateIoCompletionPort((HANDLE)pClientInfo->m_socketClient, mIOCPHandle, (ULONG_PTR)(pClientInfo), 0);

		if (hIOCP == NULL || mIOCPHandle != hIOCP) {
			GetLastErrorAsString();
			return false;
		}

		return true;
	}

	bool bindRecv(stClientInfo* pClientInfo)
	{
		DWORD dwFlag = 0;
		DWORD dwRecvNumBytes = 0;

		// settings for Overlapped IO object
		pClientInfo->m_stRecvOverlappedEx.m_wsaBuf.len = MAX_SOCKBUF;
		pClientInfo->m_stRecvOverlappedEx.m_wsaBuf.buf = pClientInfo->m_stRecvOverlappedEx.m_szBuf;
		pClientInfo->m_stRecvOverlappedEx.m_Operation = IOOperation::RECV;

		int bRet = WSARecv(pClientInfo->m_socketClient,
			&(pClientInfo->m_stRecvOverlappedEx.m_wsaBuf),
			1,
			&dwRecvNumBytes,
			&dwFlag,
			(LPWSAOVERLAPPED) & (pClientInfo->m_stRecvOverlappedEx),
			NULL);

		if (bRet == SOCKET_ERROR || (WSAGetLastError() != ERROR_IO_PENDING))
		{
			GetLastErrorAsString();
			return false;
		}
		return true;
	}

	bool SendMsg(stClientInfo* pClientInfo, char *pMsg, int m_Len)
	{
		DWORD dwSendNumbytes = 0;

		CopyMemory(pClientInfo->m_stSendOverlappedEx.m_szBuf, pMsg, m_Len);
		pClientInfo->m_stSendOverlappedEx.m_wsaBuf.len = m_Len;
		pClientInfo->m_stSendOverlappedEx.m_wsaBuf.buf = pClientInfo->m_stSendOverlappedEx.m_szBuf;
		pClientInfo->m_stSendOverlappedEx.m_Operation = IOOperation::SEND;

		int nRet = WSASend(pClientInfo->m_socketClient,
			&(pClientInfo->m_stSendOverlappedEx.m_wsaBuf),
			1,
			&dwSendNumbytes,
			0,
			(LPWSAOVERLAPPED)&(pClientInfo->m_stSendOverlappedEx),
			NULL);

		if (nRet == SOCKET_ERROR || (WSAGetLastError() != ERROR_IO_PENDING))
		{
			GetLastErrorAsString();
			return false;
		}
		return true;
	}

	void WorkerThread()
	{
		stClientInfo* pClientInfo = nullptr; // Completion IO Key
		bool bSuccess = true;
		DWORD dwIOSize = 0;
		LPOVERLAPPED lpOverlappedIO = NULL; // lpOverlappedIO struct

		while (m_isWorkerRun)
		{
			bSuccess = GetQueuedCompletionStatus(mIOCPHandle,
				&dwIOSize,
				(PULONG_PTR)&(pClientInfo),
				&lpOverlappedIO,
				INFINITE);

			if (bSuccess == true && dwIOSize == 0 && lpOverlappedIO == NULL)
			{
				m_isWorkerRun = false;
				continue;
			}

			if (lpOverlappedIO == NULL)
			{
				continue;
			}

			// if client send close
			if (bSuccess == false || ((dwIOSize) == 0 && bSuccess == true)) {
				printf("socket(%d) close \n", (int)pClientInfo->m_socketClient);
				CloseSocket(pClientInfo);
				continue;
			}

			stOverlappedEx* pOverlappedEx = (stOverlappedEx*)lpOverlappedIO;

			if (pOverlappedEx->m_Operation == IOOperation::RECV)
			{
				pOverlappedEx->m_szBuf[dwIOSize] = NULL;
				printf("Receive bytes : %d , msg : %s\n", dwIOSize, pOverlappedEx->m_szBuf);

				// Echo message to client
				SendMsg(pClientInfo, pOverlappedEx->m_szBuf, dwIOSize);
				bindRecv(pClientInfo);
			}
			else if (pOverlappedEx->m_Operation == IOOperation::SEND)
			{
				printf("Send bytes : %d , msg : %s\n", dwIOSize, pOverlappedEx->m_szBuf);
			}
			else
			{
				printf("socket(%d) error occured . \n", (int)pClientInfo->m_socketClient);
			}
		}
	}

	void AcceptThread()
	{
		SOCKADDR_IN		stClientAddr;
		int nAddrLen = sizeof(SOCKADDR_IN);

		while (m_isAcceptRun)
		{
			stClientInfo* pClientInfo = GetEmptyClientInfo();
			if (pClientInfo == NULL) {
				std::cout << "client Recv Error " << std::endl;
				GetLastErrorAsString();
				return;
			}

			pClientInfo->m_socketClient = accept(mListenSocket, (SOCKADDR*)&stClientAddr, &nAddrLen);
			if (pClientInfo->m_socketClient == INVALID_SOCKET)
			{
				GetLastErrorAsString();
				return;
			}

			bool bRet = bindIOCompletionPort(pClientInfo);
			if (bRet == false) {
				return;
			}

			bRet = bindRecv(pClientInfo);
			if (bRet == false) {
				return;
			}

			char clientIP[32] = { 0, };
			inet_ntop(AF_INET, &(stClientAddr.sin_addr), clientIP, 32 - 1);
			printf("Client Connected : IP(%s) SOCKET(%d)\n", clientIP, (int)pClientInfo->m_socketClient);

			// add number of client 
			++mClientCnt;
		}
	}

	void CloseSocket(stClientInfo* pClientInfo, bool isForce = true)
	{
		auto clientIndex = pClientInfo->mIndex;

		struct linger stLinger = { 0, };
		if (isForce == true) {
			stLinger.l_onoff = 1;
		}

		shutdown(pClientInfo->m_socketClient, SD_BOTH);
		setsockopt(pClientInfo->m_socketClient, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof(stLinger));
		closesocket(pClientInfo->m_socketClient);
		pClientInfo->m_socketClient = INVALID_SOCKET;

		OnClose(clientIndex);
	}

private:

	SOCKET			mListenSocket = INVALID_SOCKET;
	std::vector<stClientInfo>		mClientInfos;
	int				mClientCnt = 0;
	std::vector<std::thread>	mIOWorkerThread;
	std::thread					mAcceptThread;
	HANDLE						mIOCPHandle = INVALID_HANDLE_VALUE;
	bool						m_isWorkerRun = true;
	bool						m_isAcceptRun = true;
	char						m_sockBuf[1024] = { 0, };
};