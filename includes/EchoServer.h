#pragma once

#include "IOCPServer.h"
#include "Packet.h"

class EchoServer : public IOCPServer
{
public:
	EchoServer() = default;
	virtual ~EchoServer() = default;

	void OnConnect(const UINT32 clientIndex_) override;
	void OnClose(const UINT32 clientIndex_) override;
	void OnReceive(const UINT32 clientIndex_, const UINT32 size_, shared_ptr<char[]> pData_) override;

	void Run(const UINT32 maxClient);

	void End();

private:
	void ProcessPacket();

	PacketData DequePacketData();

private:
	bool mIsRunProcessThread = false;

	thread mProcessThread;

	mutex mLock;
	deque<PacketData> mPacketDataQueue;
};

