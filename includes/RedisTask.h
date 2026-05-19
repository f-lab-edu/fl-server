#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <memory>

#include "ErrorCode.h"
#include "RedisTaskID.h"

struct RedisTask
{
	UINT32 UserIndex = { 0 };
	RedisTaskID::Enum TaskID = { RedisTaskID::INVALID };
	UINT16 DataSize = { 0 };
	shared_ptr<void> pData;

	void Release()
	{
		pData.reset();
	}
};