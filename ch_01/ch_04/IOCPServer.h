#pragma once
#pragma comment(lib,"ws2_32")

#include "ClientInfo.h"
#include "Define.h"
#include <thread>
#include <vector>

class IOCPServer
{
public:
	IOCPServer(void) { }

	virtual ~IOCPServer() {
		WSACleanup();
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

	bool BindAndListen(int nBindPort)
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

	bool SendMsg(const UINT32 sessionIndex_, const UINT32 dataSize_, char* pData)
	{
		auto pClient = GetClientInfo(sessionIndex_);
		return pClient->SendMsg(dataSize_, pData);
	}

	virtual void OnConnect(const UINT32 clientIndex_) {}
	virtual void OnClose(const UINT32 clientIndex_) {}
	virtual void OnReceive(const UINT32 clientIndex_, const UINT32 size_, char* pData_) {}

private:
	void CreateClient(const UINT32 maxClientCnt)
	{
		for (UINT32 i = 0; i < maxClientCnt; i++)
		{
			mClientInfos.emplace_back();
			mClientInfos[i].Init(i);
		}
	}

	bool CreateWorkerThread()
	{
		unsigned int uiThreadId = 0;
		for (int i = 0; i < MAX_WORKERTHREAD; i++)
		{
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
		for (auto&c : mClientInfos)
		{
			if (c.IsConnected() == false) {
				return &c;
			}
		}
		return nullptr;
	}

	stClientInfo* GetClientInfo(const UINT32 sessionIndex)
	{
		return &mClientInfos[sessionIndex];
	}

	void WorkerThread()
	{
		stClientInfo*				pClientInfo = nullptr;
		bool						bSuccess = true;
		DWORD						dwIOSize = 0;
		LPOVERLAPPED				lpOverlapped = NULL;

		while (m_isWorkerRun)
		{
			bSuccess = GetQueuedCompletionStatus(mIOCPHandle,
				&dwIOSize,
				(PULONG_PTR)&pClientInfo,
				&lpOverlapped,
				INFINITE);

			if (bSuccess == true && dwIOSize == 0 && lpOverlapped == NULL)
			{
				m_isWorkerRun = false;
				continue;
			}

			if (lpOverlapped == NULL)
			{
				continue;
			}

			// if client shutdown
			if (bSuccess == false || (dwIOSize == 0 && bSuccess == true))
			{
				CloseSocket(pClientInfo, true);
				continue;
			}

			auto pOverlappedEx = (stOverlappedEx*)lpOverlapped;

			if (pOverlappedEx->m_Operation == IOOperation::RECV)
			{
				OnReceive(pClientInfo->GetIndex(), dwIOSize, pClientInfo->RecvBuffer());

				pClientInfo->BindRecv();
			}
			else if (pOverlappedEx->m_Operation == IOOperation::SEND)
			{
				delete[] pOverlappedEx->m_wsaBuf.buf;
				delete pOverlappedEx;
				pClientInfo->SendComplete(dwIOSize);
			}
			else
			{
				printf("socket(%d) error occured . \n", (int)pClientInfo->GetIndex());
			}
		}
	}

	void AcceptThread()
	{
		SOCKADDR_IN			stClientAddr;
		int					nAddrLen = sizeof(SOCKADDR_IN);

		while (m_isAcceptRun)
		{
			stClientInfo* pClientInfo = GetEmptyClientInfo();
			if (pClientInfo == NULL)
			{
				std::cout << " Client is Full " << std::endl;
				return;
			}

			auto newSocket = accept(mListenSocket, (SOCKADDR*)&stClientAddr, &nAddrLen);
			
			if (newSocket == INVALID_SOCKET)
			{
				continue;
			}
			
			if (pClientInfo->OnConnect(mIOCPHandle, newSocket) == false)
			{
				pClientInfo->Close(true);
				return;
			}

			OnConnect(pClientInfo->GetIndex());
			pClientInfo->BindRecv();
			// Increase num of connected Client Number
			++mClientCnt;
		}
	}

	void CloseSocket(stClientInfo* pClientInfo, bool bisForce = true)
	{
		auto clientIndex = pClientInfo->GetIndex();
		pClientInfo->Close(bisForce);
		OnClose(clientIndex);
	}
private:

	SOCKET						mListenSocket = INVALID_SOCKET;
	std::vector<stClientInfo>	mClientInfos;
	int							mClientCnt = 0;
	std::vector<std::thread>	mIOWorkerThread;
	std::thread					mAcceptThread;
	HANDLE						mIOCPHandle = INVALID_HANDLE_VALUE;
	bool						m_isWorkerRun = true;
	bool						m_isAcceptRun = true;

};