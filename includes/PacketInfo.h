#pragma once
#include <windows.h>
#include <memory>

struct PacketInfo
{
	UINT32 ClientIndex = { 0 };
	UINT16 PacketId = { 0 };
	UINT16 DataSize = { 0 };
	shared_ptr<char> pDataPtr;
};
