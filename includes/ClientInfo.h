#pragma once

#include "Define.h"
#include "OverlappedEx.h"

struct stClientInfo
{
	INT32 mIndex = { 0 };
	SOCKET m_socketClient;
	stOverlappedEx m_stRecvOverlappedEx;
	stOverlappedEx m_stSendOverlappedEx;

	char mRecvBuf[MAX_SOCKBUF] = {};
	char mSendBuf[MAX_SOCKBUF] = {};

	stClientInfo()
	{
		ZeroMemory(&m_stRecvOverlappedEx, sizeof(stOverlappedEx));
		ZeroMemory(&m_stSendOverlappedEx, sizeof(stOverlappedEx));
		m_socketClient = INVALID_SOCKET;
		ZeroMemory(mRecvBuf, MAX_SOCKBUF);
		ZeroMemory(mSendBuf, MAX_SOCKBUF);
	}
};