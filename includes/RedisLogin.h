#pragma once

#include <windows.h>

#include "Define.h"
#include "ErrorCode.h"

#pragma pack(push, 1)
struct RedisLoginReq
{
	char UserID[MAX_USER_ID_LEN + 1];
	char UserPW[MAX_USER_PW_LEN + 1];
};

struct RedisLoginRes
{
	UINT16 Result = (UINT16)ERROR_CODE::NONE;
};
#pragma pack(pop)