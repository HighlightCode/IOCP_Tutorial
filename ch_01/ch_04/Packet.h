#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>


/*-------------------
	  PacketData
--------------------*/
struct PacketData
{
	UINT32	SessionIdx = 0;
	UINT32	DataSize = 0;
	char* pPacketData = nullptr;

	void Set(PacketData& v)
	{
		SessionIdx = v.SessionIdx;
		DataSize = v.DataSize;

		pPacketData = new char[v.DataSize];
		CopyMemory(pPacketData, v.pPacketData, v.DataSize);
	}

	void Set(UINT32 SessionIndex_, UINT32 DataSize_, char* pData_)
	{
		SessionIdx = SessionIndex_;
		DataSize = DataSize_;
		pPacketData = new char[DataSize];
		CopyMemory(pPacketData, pData_, DataSize);
	}

	void Release()
	{
		delete pPacketData;
	}
};