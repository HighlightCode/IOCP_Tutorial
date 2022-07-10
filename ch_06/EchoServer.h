#pragma once

#include "IOCPServer.h"
#include "Packet.h"

#include <vector>
#include <deque>
#include <thread>
#include <mutex>


/*-------------------
*	  Echo Server
--------------------*/
class EchoServer : public IOCPServer
{
public:
	EchoServer() = default;
	virtual ~EchoServer() = default;

	virtual void OnConnect(const UINT32 ClientIndex_) override
	{
		std::cout << ClientIndex_ << " Client Connect . " << std::endl;
	}

	virtual void OnClose(const UINT32 ClientIndex_) override
	{
		std::cout << ClientIndex_ << " Client Closed . " << std::endl;
	}

	virtual void OnReceive(const UINT32 ClientIndex_, const UINT32 dataSize_, char *pData) override
	{
		std::cout << ClientIndex_ << "Client Received, data size : " << pData << std::endl;

		PacketData packet;
		packet.Set(ClientIndex_, dataSize_, pData);
		mPacketDataQueue.push_back(packet);
	}

	void Run(const UINT32 maxClient)
	{
		mIsRunProcessThread = true;
		mProcessThread = std::thread([this]() {ProcessPacket(); });

		StartServer(maxClient);
	}

	void End()
	{
		mIsRunProcessThread = true;

		if (mProcessThread.joinable())
		{
			mProcessThread.join();
		}

		DestroyThread();
	}

private:
	void ProcessPacket()
	{
		while (mIsRunProcessThread)
		{
			auto packetData = DequeuePacketData();
			if (packetData.DataSize != 0)
			{
				SendMsg(packetData.SessionIndex, packetData.DataSize, packetData.pPacketData);
			}
			else
			{
				std::this_thread::sleep_for(std::chrono::microseconds(1));
			}
		}
	}

	PacketData DequeuePacketData()
	{
		PacketData packetData;

		std::lock_guard<std::mutex> guard(mLock);
		if (mPacketDataQueue.empty()) 
		{
			return PacketData();
		}

		packetData.Set(mPacketDataQueue.front());

		mPacketDataQueue.front().Release();
		mPacketDataQueue.pop_front();

		return packetData;
	}

private:
	bool mIsRunProcessThread = false;

	std::thread mProcessThread;

	std::mutex mLock;

	std::deque<PacketData> mPacketDataQueue;
};