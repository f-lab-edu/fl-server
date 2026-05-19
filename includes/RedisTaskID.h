#pragma once

struct RedisTaskID
{
	enum Enum
	{
		INVALID = 0,

		REQUEST_LOGIN = 1001,
		RESPONSE_LOGIN = 1002,
	};
};