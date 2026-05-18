#pragma once

#define WIN32_LEAN_AND_MEAN

#include "PacketInfo.h"

#pragma pack(push,1)
#include "LoginPacket.h"
#include "RoomPacket.h"
#include "RoomChatPacket.h"
#pragma pack(pop)

struct PacketData
{
	UINT32 ClientIndex = { 0 };
	UINT32 DataSize = { 0 };
	shared_ptr<char[]> pPacketData = {nullptr};

	void Set(PacketData& value);

	void Set(UINT32 sessionIndex_, UINT32 dataSize_, shared_ptr<char[]> pData);

	void Release();
};

