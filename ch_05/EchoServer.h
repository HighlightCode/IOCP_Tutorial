#pragma once

#include "IOCPServer.h"
#include "Packet.h"

#include <vector>
#include <deque>
#include <thread>
#include <mutex>


class EchoServer : public IOCPServer
{
public:

	EchoServer() = default;
	virtual ~EchoServer() = default;

	virtual void OnConnect(const UINT32 clientIndex_) override
	{
		std::cout << "Client connected : " << clientIndex_ << std::endl;
	}

	virtual void OnClose(const UINT32 clientIndex_) override
	{
		std::cout << "Client closed : " << clientIndex_ << std::endl;
	}

	virtual void OnReceive(const UINT32 clientIndex_, const UINT32 size_, char* pData_)
	{
		PacketData packet;
		packet.Set(clientIndex_, size_, pData_);
		std::lock_guard<std::mutex> guard(mLock);
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
		mIsRunProcessThread = false;

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
				std::cout << "packet data : " << packetData.pPacketData << std::endl;
				SendMsg(packetData.SessionIndex,packetData.DataSize, packetData.pPacketData);
			}
			else
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
	}

	PacketData DequeuePacketData()
	{
		PacketData packetData;

		std::lock_guard<std::mutex> guard(mLock);
		if (mPacketDataQueue.empty())
		{
			return packetData;
		}

		packetData.Set(mPacketDataQueue.front());
		mPacketDataQueue.front().Release();
		mPacketDataQueue.pop_front();
		
		return packetData;
	}

	bool mIsRunProcessThread = false;

	std::thread	mProcessThread;

	std::mutex mLock;

	std::deque<PacketData> mPacketDataQueue;
};