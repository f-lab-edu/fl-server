#include "pch.h"

#include "ClientInfo.h"

stClientInfo::stClientInfo()
{
	ZeroMemory(&mRecvOverlappedEx, sizeof(stOverlappedEx));
	mSock = INVALID_SOCKET;
	ZeroMemory(mRecvBuf.get(), MAX_SOCKBUF);
}

void stClientInfo::Init(const UINT32 index)
{
	mIndex = index;
}

bool stClientInfo::OnConnect(HANDLE iocpHandle_, SOCKET socket_)
{
	mSock = socket_;

	Clear();

	if (BindIOCompletionPort(iocpHandle_) == false)
	{
		return false;
	}

	return BindRecv();
}

void stClientInfo::Close(bool bIsForce)
{
	linger stLinger = { 0, 0 }; // SO_DONTLINGER

	// bIsForce == true :: SO_LINGER, timeout = 0 // force shutdown, warning : data lost
	if (bIsForce == true)
	{
		stLinger.l_onoff = 1;
	}

	shutdown(mSock, SD_BOTH);

	setsockopt(mSock, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof(stLinger));

	closesocket(mSock);

	mSock = INVALID_SOCKET;
}

void stClientInfo::Clear()
{
	mSendPos = 0;
	mIsSending = false;
}

bool stClientInfo::BindIOCompletionPort(HANDLE iocpHandle_)
{
	auto hIOCP = CreateIoCompletionPort((HANDLE)GetSock(),
		iocpHandle_,
		(ULONG_PTR)(this),
		0);

	if (hIOCP == INVALID_HANDLE_VALUE)
	{
		printf("[ERROR] CreateIoCompletionPort() failed: %d", GetLastError());
		return false;
	}

	return true;
}

bool stClientInfo::BindRecv()
{
	DWORD dwFlag = { 0 };
	DWORD dwRecvNumBytes = { 0 };

	mRecvOverlappedEx.m_wsaBuf.len = MAX_SOCKBUF;
	mRecvOverlappedEx.m_wsaBuf.buf = mRecvBuf.get();
	mRecvOverlappedEx.m_eOperation = IOOperation::RECV;

	int nRet = WSARecv(mSock,
		&(mRecvOverlappedEx.m_wsaBuf),
		1,
		&dwRecvNumBytes,
		&dwFlag,
		(LPWSAOVERLAPPED) & (mRecvOverlappedEx),
		NULL);

	if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
	{
		printf("[ERROR] WSARecv() failed : %d\n", WSAGetLastError());
		return false;
	}

	return true;
}

bool stClientInfo::SendMsg(const UINT32 dataSize_, shared_ptr<char[]> pMsg_)
{
	lock_guard<mutex> guard(mSendLock);

	if ((mSendPos + dataSize_) > MAX_SOCK_SENDBUF)
		mSendPos = 0;

	auto pSendBuf = &mSendBuf[mSendPos];

	CopyMemory(pSendBuf, pMsg_.get(), dataSize_);
	mSendPos += dataSize_;

	return true;
}

bool stClientInfo::SendIO()
{
	if (mSendPos <= 0 || mIsSending)
		return true;

	lock_guard<mutex> guard(mSendLock);

	mIsSending = true;

	CopyMemory(mSendingBuf, &mSendBuf[0], mSendPos);

	mSendOverlappedEx.m_wsaBuf.len = mSendPos;
	mSendOverlappedEx.m_wsaBuf.buf = &mSendingBuf[0];
	mSendOverlappedEx.m_eOperation = IOOperation::SEND;

	DWORD dwRecvNumBytes = { 0 };
	int nRet = WSASend(mSock,
		&(mSendOverlappedEx.m_wsaBuf),
		1,
		&dwRecvNumBytes,
		0,
		(LPWSAOVERLAPPED) & (mSendOverlappedEx),
		NULL);

	if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
	{
		printf("[ERROR] WSASend() failed : %d\n", WSAGetLastError());
		return false;
	}

	mSendPos = 0;
	return true;
}

void stClientInfo::SendCompleted(const UINT32 dataSize_)
{
	mIsSending = false;
	printf("[Send Completed] bytes : %d\n", dataSize_);
}
