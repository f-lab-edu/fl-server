#pragma once

#include <winsock2.h>
#include <Ws2tcpip.h>

#include "Define.h"

#include "OverlappedEx.h"

struct stClientInfo
{
public:
	stClientInfo()
	{
		ZeroMemory(&mRecvOverlappedEx, sizeof(stOverlappedEx));
		mSock = INVALID_SOCKET;
		ZeroMemory(mRecvBuf.get(), MAX_SOCKBUF);
	}

	void Init(const UINT32 index);
	
	UINT32 GetIndex() { return mIndex; }

	bool IsConnected() { return mSock != INVALID_SOCKET; }

	SOCKET GetSock() { return mSock; }

	shared_ptr<char[]> RecvBuffer() { return mRecvBuf; }

	bool OnConnect(HANDLE iocpHandle_, SOCKET socket_);

	void Close(bool bIsForce = false);

	void Clear();

	bool BindIOCompletionPort(HANDLE iocpHandle_);
	
	bool BindRecv();

	bool SendMsg(const UINT32 dataSize_, shared_ptr<char[]> pMsg_);

	void SendCompleted(const UINT32 dataSize_);

private:
	INT32 mIndex = { 0 };
	SOCKET mSock;
	stOverlappedEx mRecvOverlappedEx;

	shared_ptr<char[MAX_SOCKBUF]> mRecvBuf = {};
};