#pragma once
#pragma comment(lib,"ws2_32")

#include <winsock2.h>
#include <Ws2tcpip.h>

#include <thread>
#include <string>
#include <iostream>
#include <vector>


#define MAX_SOCKBUF 1024
#define MAX_WORKTHREAD 4

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

// IOOperation 의 타입 정의.
enum class IOOperation
{
	RECV,
	SEND
};

struct stOverlappedEx
{
	WSAOVERLAPPED		wsaoverlapped;			// Overlapped IO 구조체
	SOCKET				m_socket;				// client Socket
	WSABUF				m_wsaBuf;				// OverlappedIO 구조체를 위한 버퍼
	char				m_szBuf[MAX_SOCKBUF];	// 데이터 buffer
	IOOperation			m_Operation;			//operation 타입 종류
};

struct stClientInfo
{
	SOCKET				m_socketClient;
	stOverlappedEx		m_RecvOverlappedEx;
	stOverlappedEx		m_SendOverlappedEx;

	stClientInfo()
	{
		m_socketClient = INVALID_SOCKET;
		ZeroMemory(&m_RecvOverlappedEx, sizeof(m_RecvOverlappedEx));
		ZeroMemory(&m_SendOverlappedEx, sizeof(m_SendOverlappedEx));
	}
};

class IOCompletionPort
{
public:
	IOCompletionPort(void) {};

	~IOCompletionPort(void)
	{
		WSACleanup();
	}

	// Initialize Socket;
	bool InitSocket()
	{
		WSADATA wsaData;
		int err = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (err != 0) {
			/* Can't startup WINSOCK DLL*/
			std::cout << "Can't initialize winsock : " << err << std::endl;
			return false;
		}

		m_listenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
		if (m_listenSocket == INVALID_SOCKET)
		{
			/* socket err*/
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
		stServAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); //socket addr to local host "127.0.0.1"

		int err = bind(m_listenSocket, (SOCKADDR*)&stServAddr, sizeof(SOCKADDR_IN));
		if (err != 0) 
		{
			std::cout << "bind error occured : " << err << std::endl;
			return false;
		}

		// 접속 요청을 위한 function
		err = listen(m_listenSocket, 10);
		if (err != 0)
		{
			std::cout << "listen failed : " << err << std::endl;
			return false;
		}

		std::cout << "socket bind successfully : " << nBindPort << std::endl;

		return true;
	}

	// 접속 요청을 수락하고 메시지를 받는 함수..
	bool StartServer(const UINT32 maxClientCount)
	{
		CreateClient(maxClientCount);

		// Create IOCP Handle Object 
		m_IOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, MAX_WORKTHREAD);
		if (m_IOCPHandle == NULL)
		{
			GetLastErrorAsString();
		}
		
		// create Worker Thread
		bool b_ret = CreateWorkerThread();
		if (b_ret == false)
		{
			return false;
		}

		// create Accept Thread
		b_ret = CreateAcceptThread();
		if (b_ret == false)
		{
			return false;
		}

		return true;
	}

	// Destroy the working thread;
	void DestroyThread()
	{
		m_isWorkerRun = false;
		CloseHandle(m_IOCPHandle);

		for (auto& th : m_IOWorkerThread)
		{
			if (th.joinable())
			{
				th.join();
			}
		}

		//Accepter 쓰레드를 종요한다.
		m_isAccpetRun = false;
		closesocket(m_listenSocket);

		if (m_AcceptThread.joinable())
		{
			m_AcceptThread.join();
		}
	}

private:

	void CreateClient(const UINT32 maxClientCount)
	{
		for (UINT32 i = 0; i < maxClientCount; i++)
		{
			m_ClientInfo.emplace_back();
		}
	}

	bool CreateWorkerThread() 
	{
		unsigned int uiThreadId = 0;
		for (int i = 0; i < MAX_WORKTHREAD; i++)
		{
			m_IOWorkerThread.emplace_back([this]() {WorkerThread(); });
		}
		std::cout << "Worker Thread Init " << std::endl;
		return true;
	}

	bool CreateAcceptThread()
	{
		m_AcceptThread = std::thread([this]() {AcceptThread(); });
		std::cout << "Accepter Thread Init " << std::endl;
		return true;
	}

	// return empty stClientInfo
	stClientInfo* GetEmptyClientInfo()
	{
		for (auto& c : m_ClientInfo)
		{
			if (c.m_socketClient == INVALID_SOCKET)
			{
				return &c;
			}
		}
		return nullptr;
	}

	// bind IOCompletionPort and Completion Key ..
	bool bindIOCompletionPort(stClientInfo* pClientInfo)
	{
		auto hIOCP = CreateIoCompletionPort((HANDLE)pClientInfo->m_socketClient, m_IOCPHandle, (ULONG_PTR)(pClientInfo), 0);

		if (hIOCP == NULL || m_IOCPHandle != hIOCP)
		{
			GetLastErrorAsString();
			return false;
		}
		return true;
	}

	// WSARecv 의 비동기 작업.
	bool bindRecv(stClientInfo* pClientInfo)
	{
		DWORD dwFlag = 0;
		DWORD dwRecvNumBytes = 0;

		// settings for Overlapped IO object
		pClientInfo->m_RecvOverlappedEx.m_wsaBuf.len = MAX_SOCKBUF;
		pClientInfo->m_RecvOverlappedEx.m_wsaBuf.buf = pClientInfo->m_RecvOverlappedEx.m_szBuf;
		pClientInfo->m_RecvOverlappedEx.m_Operation = IOOperation::RECV;

		int nRet = WSARecv(pClientInfo->m_socketClient,
				&(pClientInfo->m_RecvOverlappedEx.m_wsaBuf),
				1,
				&dwRecvNumBytes,
				&dwFlag,
				(LPWSAOVERLAPPED) & (pClientInfo->m_RecvOverlappedEx),
				NULL);

		if (nRet == SOCKET_ERROR || (WSAGetLastError() != ERROR_IO_PENDING))
		{
			GetLastErrorAsString();
			return false;
		}
		return true;
	}

	// WSASend 의 비동기 작업
	bool SendMsg(stClientInfo* pClientInfo, char *pMsg, int nLen)
	{
		DWORD dwRecvNumBytes = 0;

		CopyMemory(pClientInfo->m_SendOverlappedEx.m_szBuf, pMsg, nLen);
		pClientInfo->m_SendOverlappedEx.m_wsaBuf.len = nLen;
		pClientInfo->m_SendOverlappedEx.m_wsaBuf.buf = pClientInfo->m_SendOverlappedEx.m_szBuf;
		pClientInfo->m_SendOverlappedEx.m_Operation = IOOperation::SEND;

		int nRet = WSASend(pClientInfo->m_socketClient,
						(LPWSABUF)&(pClientInfo->m_SendOverlappedEx.m_wsaBuf),
						1,
						&dwRecvNumBytes,
						0,
						(LPWSAOVERLAPPED)&(pClientInfo->m_SendOverlappedEx),
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
		stClientInfo* pClientInfo = nullptr; // Completion KEY 
		bool bSuccess = true;
		DWORD dwIoSize = 0;
		LPOVERLAPPED lpOverlappedIO = NULL; // lpOverlappedIO struct

		while (m_isWorkerRun)
		{
			bSuccess = GetQueuedCompletionStatus(m_IOCPHandle, 
											&dwIoSize, 
											(PULONG_PTR)&(pClientInfo), 
											&lpOverlappedIO, 
											INFINITE);
			if (bSuccess == true && dwIoSize == 0 && lpOverlappedIO == NULL)
			{
				m_isWorkerRun = false;
				continue;
			}

			if (lpOverlappedIO == NULL)
			{
				continue;
			}

			stOverlappedEx* pOverlappedEx = (stOverlappedEx*)lpOverlappedIO;

			// overlapped IO 결과 처리
			if (pOverlappedEx->m_Operation == IOOperation::RECV)
			{
				pOverlappedEx->m_szBuf[dwIoSize] = NULL;
				printf("Receive bytes : %d , msg : %s\n", dwIoSize, pOverlappedEx->m_szBuf);

				// echo message to client
				SendMsg(pClientInfo, pOverlappedEx->m_szBuf, dwIoSize);
				bindRecv(pClientInfo);
			}
			else if (pOverlappedEx->m_Operation == IOOperation::SEND)
			{
				printf("Send bytes : %d , msg : %s\n", dwIoSize, pOverlappedEx->m_szBuf);
			}
			else 
			{
				printf("socket(%d) error occured . \n", (int)pClientInfo->m_socketClient);
			}
		}
	}

	void AcceptThread()
	{
		SOCKADDR_IN			stClientAddr;
		int nAddrLen = sizeof(SOCKADDR_IN);

		while (m_isAccpetRun)
		{
			stClientInfo* pClientInfo = GetEmptyClientInfo();
			if (pClientInfo == NULL)
			{
				std::cout << "client Recv Error " << std::endl;
				GetLastErrorAsString();
				return;
			}

			pClientInfo->m_socketClient = accept(m_listenSocket, (SOCKADDR*)&stClientAddr, &nAddrLen);

			if (pClientInfo->m_socketClient == INVALID_SOCKET)
			{
				GetLastErrorAsString();
				return;
			}

			bool bRet = bindIOCompletionPort(pClientInfo);
			if (false == bRet)
			{
				return;
			}

			bRet = bindRecv(pClientInfo);
			if (false == bRet)
			{
				return;
			}

			char clientIP[32] = { 0, };
			inet_ntop(AF_INET, &(stClientAddr.sin_addr), clientIP, 32 - 1);
			printf("Client Connected : IP(%s) SOCKET(%d)\n", clientIP, (int)pClientInfo->m_socketClient);

			//클라이언트 갯수 증가
			++mClientCnt;
		}
	}

	void CloseSocket(stClientInfo* pClientInfo, bool bIsForce = false)
	{
		struct linger stLinger = { 0,0 };
		if (true == bIsForce)
		{
			stLinger.l_onoff = 1;
		}

		shutdown(pClientInfo->m_socketClient, SD_BOTH);

		setsockopt(pClientInfo->m_socketClient, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof(stLinger));

		closesocket(pClientInfo->m_socketClient);

		pClientInfo->m_socketClient = INVALID_SOCKET;
	}

	SOCKET							m_listenSocket;			// listening socket
	std::vector<stClientInfo>		m_ClientInfo;			// client count 
	int								mClientCnt = 0;			// current Client count 
	std::vector<std::thread>		m_IOWorkerThread;		//	worker thread
	std::thread						m_AcceptThread;
	HANDLE							m_IOCPHandle = INVALID_HANDLE_VALUE;
	bool							m_isWorkerRun = true;
	bool							m_isAccpetRun = true;
	char							m_sockBuf[1024] = { 0, };
};