#include "pch.h"

#include "ChatServer.h"

void ChatServer::Run(const UINT32 maxClient)
{
	auto sendPacketFunc = [&](UINT32 clientIndex_, UINT16 packetSize, shared_ptr<char[]> pSendPacket)
		{
			SendMsg(clientIndex_, packetSize, pSendPacket);
		};

	m_pPacketManager = make_unique<PacketManager>();
	m_pPacketManager->SendPacketFunc = sendPacketFunc;
	m_pPacketManager->Init(maxClient);
	m_pPacketManager->Run();

	StartServer(maxClient);
}

void ChatServer::End()
{
	m_pPacketManager->End();

	DestroyThread();
}

void ChatServer::OnConnect(const UINT32 clientIndex_)
{
	printf("[OnConnect] client : index(%d)\n", clientIndex_);

	PacketInfo packet{ clientIndex_, (UINT16)PACKET_ID::SYS_USER_CONNECT, 0 };
	m_pPacketManager->PushSystemPacket(packet);
}

void ChatServer::OnClose(const UINT32 clientIndex_)
{
	printf("[OnClose] client : index(%d)\n", clientIndex_);

	PacketInfo packet{ clientIndex_, (UINT16)PACKET_ID::SYS_USER_DISCONNECT, 0 };
	m_pPacketManager->PushSystemPacket(packet);
}

void ChatServer::OnReceive(const UINT32 clientIndex_, const UINT32 size_, shared_ptr<char[]> pData_)
{
	printf("[OnReceive] client : index (%d), dataSize(%d)\n", clientIndex_, size_);

	m_pPacketManager->ReceivePacketData(clientIndex_, size_, pData_);
}
