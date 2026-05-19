#include "CRedisConn.h"

namespace RedisCpp
{
	const char* CRedisConn::_errDes[ERR_BOTTOM] =
	{
			"No error.",
			"NULL pointer ",
			"No connection to the redis server.",
					"Inser Error,pivot not found.",
					"key not found",
			"hash field not found",
			"error index"
	};
}
