#pragma once

#include "IOCPServer.h"

class EchoServer : public IOCPServer
{
	virtual void OnConnect(const UINT32 clientIndex_) override
	{
		std::cout << "Client connected : " << clientIndex_ << std::endl;
	}

	virtual void OnClose(const UINT32 clientIndex_) override 
	{
		std::cout << "Client closed : " << clientIndex_ << std::endl;
	}

	virtual void OnReceive(const UINT32 clientIndex_, const UINT32 size_, char* pData_) override
	{
		pData_[size_] = NULL;
		std::cout << "Received from Client : " << clientIndex_ << ", msg : "<< pData_ << std::endl;
	}

};