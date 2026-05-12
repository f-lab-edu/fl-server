#pragma once
#include "IOOperation.h"

struct IOOperation;

struct stOverlappedEx
{
	WSAOVERLAPPED m_wsaOverlapped;
	SOCKET m_socketClient;
	WSABUF m_wsaBuf;
	IOOperation::Enum m_eOperation;
};