#pragma once
#pragma comment(lib,"ws2_32")

#include "ClientInfo.h"
#include "Define.h"
#include <thread>
#include <mutex>
#include <vector>

class IOCPServer
{
public:

	IOCPServer(void) {}

	virtual ~IOCPServer() 
	{
		WSACleanup();
	}

	bool InitSocket() 
	{
		WSADATA wsaData;
		int nRet = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (nRet != 0)
		{
			GetLastErrorAsString();
			return false;
		}
		
		// Create Listen SOCKET
		mListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
		if (mListenSocket == INVALID_SOCKET)
		{
			GetLastErrorAsString();
			return false;
		}

		std::cout << "INIT SOCKET SUCCESSFULLY " << std::endl;
		return true;
	}

	bool BindAndListen(int nBindPort)
	{
		SOCKADDR_IN				stServerAddr;
		stServerAddr.sin_family = AF_INET;
		stServerAddr.sin_port = htons(nBindPort);
		stServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);

		int nRet = bind(mListenSocket, (SOCKADDR*)&stServerAddr, sizeof(SOCKADDR_IN));
		if (nRet != 0)
		{
			GetLastErrorAsString();
			return false;
		}

		nRet = listen(mListenSocket, 10);
		if (nRet != 0)
		{
			GetLastErrorAsString();
			return false;
		}

		std::cout << "Bind and Listen Successfully " << std::endl;
		return true;
	}

	bool StartServer(const UINT32 maxClientCount)
	{
		CreateClient(maxClientCount);
		
		mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, MAX_WORKERTHREAD);
		if (mIOCPHandle == NULL)
		{
			GetLastErrorAsString();
			return false;
		}

		bool bRet = CreateWorkerThread();
		if (bRet == false)
		{
			GetLastErrorAsString();
			return false;
		}

		bRet = CreateAcceptThread();
		if (bRet == false)
		{
			GetLastErrorAsString();
			return false;
		}

		CreateSendThread();
		std::cout << "SERVER START \n";

		return true;
	}

	void DestroyThread()
	{
		isSenderRun = false;
		if (mSendThread.joinable()) { mSendThread.join(); }

		isAcceptRun = false;
		closesocket(mListenSocket);
		if (mAcceptThread.joinable()) { mAcceptThread.join(); }

		isWorkerRun = false;
		CloseHandle(mIOCPHandle);

		for (auto& th : mWorkerThread)
		{
			if (th.joinable())
			{
				th.join();
			}
		}

	}

	bool SendMsg(const UINT32 sessionIndex_, const UINT32 dataSize_, char* pData)
	{
		auto pClient = GetClientInfo(sessionIndex_);
		return pClient->SendMsg(dataSize_, pData);
	}

	virtual void OnConnect(const UINT32 clientIndex_) { }

	virtual void OnClose(const UINT32 clientIndex_) { }

	virtual void OnReceive(const UINT32 clientIndex_, const UINT32 size_, char* pData_) { }

private:

	void CreateClient(const UINT32 maxClientCnt)
	{
		for (UINT32 i = 0; i < maxClientCnt; i++)
		{
			auto client = new stClientInfo();
			client->Init(i);
			mClientInfos.emplace_back(client);
		}
	}

	bool CreateWorkerThread()
	{
		isWorkerRun = true;

		for (int i = 0; i < MAX_WORKERTHREAD; i++)
		{
			mWorkerThread.emplace_back([this]() {return WorkerThread(); });
		}

		std::cout << "Worker Thread Init"  << std::endl;
		return true;
	}

	bool CreateAcceptThread()
	{
		isAcceptRun = true;
		mAcceptThread = std::thread([this]() { return AcceptThread(); });
		std::cout << "Accept Thread Init" << std::endl;
		return true;
	}

	bool CreateSendThread()
	{
		isSenderRun = true;
		mSendThread = std::thread([this]() {return SendThread(); });
		std::cout << "Send Thread Init" << std::endl;
		return true;
	}

	stClientInfo* GetEmptyClient()
	{
		for (auto &c : mClientInfos)
		{
			if (c->IsConnected() == false)
			{
				return c;
			}
		}
		return nullptr;
	}

	stClientInfo* GetClientInfo(const UINT32 SessionIndex_)
	{
		return mClientInfos[SessionIndex_];
	}

	void CloseSocket(stClientInfo* pClientInfo, bool bIsForce = false)
	{
		auto ClientIndex_ = pClientInfo->GetIndex();
		pClientInfo->Close(bIsForce);
		OnClose(ClientIndex_);
	}

	void WorkerThread()
	{
		stClientInfo*				pClientInfo = nullptr;
		bool						bSuccess = true;
		DWORD						dwIOSize = 0;
		LPOVERLAPPED				lpOverlapped = NULL;

		while (isWorkerRun)
		{
			bSuccess = GetQueuedCompletionStatus(mIOCPHandle,
										&dwIOSize,
										(PULONG_PTR)&pClientInfo,
										&lpOverlapped,
										INFINITE);

			if (bSuccess == true && dwIOSize == 0 && lpOverlapped == NULL)
			{
				isWorkerRun = false;
				continue;
			}

			if (lpOverlapped == NULL)
			{
				continue;
			}

			// case when client closed
			if (bSuccess == false && (dwIOSize == 0 && bSuccess == true))
			{
				CloseSocket(pClientInfo,true);
				continue;
			}

			auto pOverlappedEx = (stOverlappedEx*)lpOverlapped;
			// RECV
			if (pOverlappedEx->m_Operation == IOOperation::RECV)
			{
				OnReceive(pClientInfo->GetIndex(), dwIOSize, pClientInfo->RecvBuffer());
				pClientInfo->BindRecv();
			}
			else if (pOverlappedEx->m_Operation == IOOperation::SEND)
			{
				pClientInfo->SendCompleted(dwIOSize);
			}
			//예외 상황
			else
			{
				printf("Client Index(%d) error occured \n", pClientInfo->GetIndex());
			}
		}
	}

	void AcceptThread()
	{
		SOCKADDR_IN		stClientAddr;
		int nAddrLen = sizeof(SOCKADDR_IN);

		while (isAcceptRun)
		{
			stClientInfo* pClientInfo = GetEmptyClient();
			if (pClientInfo == NULL)
			{
				std::cout << "Client is Full" << std::endl;
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
			++mClientCnt;
		}
	}

	void SendThread()
	{
		while (isSenderRun)
		{
			for (auto c : mClientInfos)
			{
				if (c->IsConnected() == false)
				{
					continue;
				}
				c->SendIO();
			}
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
	}

	SOCKET						mListenSocket = INVALID_SOCKET;
	std::vector<stClientInfo*>	mClientInfos;
	int							mClientCnt = 0;
	std::vector<std::thread>	mWorkerThread;
	std::thread					mAcceptThread;
	std::thread					mSendThread;
	HANDLE						mIOCPHandle = INVALID_HANDLE_VALUE;
	bool						isWorkerRun = true;
	bool						isAcceptRun = true;
	bool						isSenderRun = true;
};