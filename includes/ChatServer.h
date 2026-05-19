#pragma once

#include "IOCPServer.h"
#include "PacketManager.h"

class PacketManager;

class ChatServer : public IOCPServer
{
public:
	ChatServer() = default;
	~ChatServer() = default;

	void Run(const UINT32 maxClient);

	void End();

	virtual void OnConnect(const UINT32 clientIndex_) override;
	virtual void OnClose(const UINT32 clientIndex_) override;
	virtual void OnReceive(const UINT32 clientIndex_, const UINT32 size_, shared_ptr<char[]> pData_) override;

private:
	unique_ptr<PacketManager> m_pPacketManager;
};

