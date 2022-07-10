#pragma once

#pragma comment(lib,"ws2_32")

#include "ClientInfo.h"
#include "Define.h"
#include <vector>
#include <thread>

class IOCPServer
{
public:
	IOCPServer(void) { }

	virtual ~IOCPServer(void)
	{
		::WSACleanup();
	}

	bool InitSocket()
	{
		WSADATA wsaData;
		
		int nRet = ::WSAStartup(MAKEWORD(2, 2), &wsaData);

		if (nRet != 0)
		{
			std::cout << " WSAStartup( ) Error , Code : " << WSAGetLastError();
			return false;
		}
		
		mListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);

		if (mListenSocket == INVALID_SOCKET)
		{
			std::cout << "SOCKET ERROR : " << WSAGetLastError() << " code." << std::endl;
		}

		return true;
	}

	bool BindAndListen(int nBindPort)
	{
		SOCKADDR_IN					stServerAddr;

		stServerAddr.sin_family = AF_INET;
		stServerAddr.sin_port = htons(nBindPort);
		stServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);

		int nRet = bind(mListenSocket, (SOCKADDR*)&stServerAddr, sizeof(SOCKADDR_IN));
		if (nRet != 0)
		{
			std::cout << "bind() error , " << WSAGetLastError() << " code ." << std::endl;
			return false;
		}

		nRet = listen(mListenSocket, 10);
		if (nRet != 0)
		{
			std::cout << "listen() error , " << WSAGetLastError() << " code ." << std::endl;
			return false;
		}

		std::cout << " server success  !" << std::endl;
		return true;
	}

	bool StartServer(const UINT32 maxClientCount)
	{
		CreateClient(maxClientCount);

		mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, MAX_WORKERTHREAD);
		if (mIOCPHandle == NULL)
		{
			std::cout << "Create IOCompletionPort Failed , Code : " << WSAGetLastError();
			return false;
		}

		bool bRet = CreateWorkerThread();
		if (bRet == false)
		{
			return false;
		}

		bRet = CreateAcceptThread();
		if (bRet == false)
		{
			return false;
		}

		std::cout << "server Start !" << std::endl;
		return true;
	}

	void DestroyThread()
	{
		mIsWorkerRun = false;
		::CloseHandle(mIOCPHandle);

		for (auto&th : mIOWorkerThreads)
		{
			if (th.joinable())
				th.join();
		}

		mIsAcceptRun = false;
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

	virtual void OnConnect(const UINT32 clientIndex_) { }
	virtual void OnClose(const UINT32 clientIndex_) { } 
	virtual void OnReceive(const UINT32 clientIndex_, const UINT32 size_, char* pData_) { }


private:
	void CreateClient(const UINT32 maxClientCount)
	{
		for (UINT32 i = 0; i < maxClientCount; i++)
		{
			auto client = new stClientInfo;
			client->Init(i);

			mClientInfos.push_back(client);
		}
	}

	bool CreateWorkerThread()
	{
		unsigned int uiThreadId = 0;

		for (int i = 0; i < MAX_WORKERTHREAD; i++)
		{
			mIOWorkerThreads.emplace_back([this]() {WorkerThread(); });
		}

		std::cout << "Init Worker Thread " << std::endl;
		return true;
	}

	stClientInfo* GetEmptyClientInfo()
	{
		for (auto& client : mClientInfos)
		{
			if (client->IsConnected() == false)
			{
				return client;
			}
		}
		return nullptr;
	}

	stClientInfo* GetClientInfo(const UINT32 clientIndex_)
	{
		return mClientInfos[clientIndex_];
	}

	bool CreateAcceptThread()
	{
		mAcceptThread = std::thread([this]() { AcceptThread(); });
		std::cout << "Init Accept Thread " << std::endl;
		return true;
	}

	void WorkerThread()
	{
		stClientInfo* pClientInfo = nullptr;
		bool bSuccess = true;
		DWORD dwIoSize = 0;
		LPOVERLAPPED lpOverlapped = NULL;

		while (mIsWorkerRun)
		{
			bSuccess = GetQueuedCompletionStatus(mIOCPHandle, &dwIoSize, (PULONG_PTR)&pClientInfo, &lpOverlapped, INFINITE);
			
			if (bSuccess == true && dwIoSize == 0 && lpOverlapped == NULL)
			{
				mIsWorkerRun = false;
				continue;
			}

			if (lpOverlapped == NULL)
			{
				continue;
			}

			if (bSuccess == false || (dwIoSize == 0 && bSuccess == true))
			{
				CloseSocket(pClientInfo);
				continue;
			}

			auto pOverlappedEx = (stOverlappedEx*)lpOverlapped;
			if (pOverlappedEx->m_IOOperation == IOOperation::RECV)
			{
				OnReceive(pClientInfo->GetIndex(), dwIoSize, pClientInfo->GetReceiveBuffer());
				pClientInfo->BindRecv();
			} 
			else if (pOverlappedEx->m_IOOperation == IOOperation::SEND)
			{
				pClientInfo->SendCompleted(dwIoSize);
			} 
			else
			{
				std::cout << pClientInfo->GetIndex() << " Client error occured, Code : " << WSAGetLastError() << std::endl;
			}
		}
	}

	void AcceptThread()
	{
		SOCKADDR_IN stClientAddr;
		int nAddrLen = sizeof(SOCKADDR_IN);

		while (mIsAcceptRun)
		{
			stClientInfo* pClientInfo = GetEmptyClientInfo();
			if (pClientInfo == nullptr)
			{
				std::cout << "Error occured client is Full " << std::endl;
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
			++mClientCnt;
		}
	}

	void CloseSocket(stClientInfo* pClientInfo, bool bIsForce = false)
	{
		auto clientIndex = pClientInfo->GetIndex();

		pClientInfo->Close(bIsForce);

		OnClose(clientIndex);
	}

	std::vector<stClientInfo*>				mClientInfos;
	SOCKET									mListenSocket = INVALID_SOCKET;
	int										mClientCnt = 0;

	std::vector<std::thread>				mIOWorkerThreads;

	std::thread								mAcceptThread;

	HANDLE									mIOCPHandle = INVALID_HANDLE_VALUE;

	bool									mIsWorkerRun = true;

	bool									mIsAcceptRun = true;
};