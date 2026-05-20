#include "pch.h"

#include "EchoServer.h"

void EchoServer::OnConnect(const UINT32 clientIndex_)
{
	spdlog::info("[OnConnect] Client: Index({})\n", clientIndex_);
}

void EchoServer::OnClose(const UINT32 clientIndex_)
{
	spdlog::info("[OnClose] Client: Index({})\n", clientIndex_);
}

void EchoServer::OnReceive(const UINT32 clientIndex_, const UINT32 size_, shared_ptr<char[]> pData_)
{
	spdlog::info("[OnReceive] Client: Index({}), dataSize({})\n", clientIndex_, size_);

	PacketData packet = {};
	packet.Set(clientIndex_, size_, pData_);

	lock_guard<mutex> guard(mLock);
	mPacketDataQueue.push_back(packet);
}

void EchoServer::Run(const UINT32 maxClient)
{
	mIsRunProcessThread = true;
	mProcessThread = thread([this]() { ProcessPacket(); });

	StartServer(maxClient);
}

void EchoServer::End()
{
	mIsRunProcessThread = false;

	if (mProcessThread.joinable())
	{
		mProcessThread.join();
	}

	DestroyThread();
}

void EchoServer::ProcessPacket()
{
	while (mIsRunProcessThread)
	{
		auto packetData = DequePacketData();
		if (packetData.DataSize != 0)
		{
			SendMsg(packetData.ClientIndex, packetData.DataSize, packetData.pPacketData);
		}
		else
		{
			this_thread::sleep_for(chrono::milliseconds(1));
		}
	}
}

PacketData EchoServer::DequePacketData()
{
	PacketData packetData = {};

	lock_guard<mutex> guard(mLock);
	if (mPacketDataQueue.empty())
	{
		return PacketData();
	}

	packetData.Set(mPacketDataQueue.front());

	mPacketDataQueue.front().Release();
	mPacketDataQueue.pop_front();

	return packetData;
}
