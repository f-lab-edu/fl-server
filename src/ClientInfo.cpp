#include "pch.h"

#include "ClientInfo.h"

stClientInfo::stClientInfo()
{
	ZeroMemory(&mRecvOverlappedEx, sizeof(stOverlappedEx));
	mSocket = INVALID_SOCKET;
	mRecvBuf = make_shared<char[]>(MAX_SOCKBUF);
	ZeroMemory(mRecvBuf.get(), MAX_SOCKBUF);
}

void stClientInfo::Init(const UINT32 index, HANDLE iocpHandle_)
{
	mIndex = index;
	mIOCPHandle = iocpHandle_;
}

bool stClientInfo::OnConnect(HANDLE iocpHandle_, SOCKET socket_)
{
	mSocket = socket_;
	mIsConnect = 1;

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

	shutdown(mSocket, SD_BOTH);

	setsockopt(mSocket, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof(stLinger));

	mIsConnect = 0;
	mLastestClosedTimeSec = chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now().time_since_epoch()).count();

	closesocket(mSocket);

	mSocket = INVALID_SOCKET;
}

void stClientInfo::Clear()
{
	mSendPos = 0;
	mIsSending = false;
}

bool stClientInfo::PostAccept(SOCKET listenSock_, const UINT64 curTimeSec_)
{
	printf_s("PostAccept. client Index: %d\n", GetIndex());

	mLastestClosedTimeSec = UINT32_MAX;

	mSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP,
		NULL, 0, WSA_FLAG_OVERLAPPED);
	if (mSocket == INVALID_SOCKET)
	{
		printf_s("[ERROR] client Socket WSASocket : %d\n", GetLastError());
		return false;
	}

	ZeroMemory(&mAcceptContext, sizeof(stOverlappedEx));

	DWORD bytes = 0;
	DWORD flags = 0;
	mAcceptContext.m_wsaBuf.len = 0;
	mAcceptContext.m_wsaBuf.buf = nullptr;
	mAcceptContext.m_eOperation = IOOperation::ACCEPT;
	mAcceptContext.SessionIndex = mIndex;

	if (FALSE == AcceptEx(listenSock_, mSocket, mAcceptBuf, 0,
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, &bytes, (LPWSAOVERLAPPED) & (mAcceptContext)))
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			printf_s("AcceptEx Error : %d\n", GetLastError());
			return false;
		}
	}

	return false;
}

bool stClientInfo::AcceptCompletion()
{
	printf_s("AcceptCompletion : SessionIndex(%d)\n", mIndex);

	if (OnConnect(mIOCPHandle, mSocket) == false)
		return false;

	SOCKADDR_IN stClientAddr;
	int nAddrLen = sizeof(SOCKADDR_IN);
	char clientIP[32] = { 0, };
	inet_ntop(AF_INET, &(stClientAddr.sin_addr), clientIP, 32 - 1);
	printf("Client Connect : IP(%s) SOCKET(%d)\n", clientIP, (int)mSocket);

	return false;
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

	int nRet = WSARecv(mSocket,
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
	int nRet = WSASend(mSocket,
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

bool stClientInfo::SetSocketOption()
{
	int opt = 1;
	if (SOCKET_ERROR == setsockopt(mSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)&opt, sizeof(int)))
	{
#ifdef _DEBUG
		printf_s("[DEBUF] TCP_NODELAY error : %d\n", GetLastError());
#endif // DEBUG
		return false;
	}

	opt = 0;
	if (SOCKET_ERROR == setsockopt(mSocket, SOL_SOCKET, SO_RCVBUF, (const char*)&opt, sizeof(int)))
	{
#ifdef _DEBUG
		printf_s("[DEBUG] SO_RCVBUF change error : %d\n", GetLastError());
#endif // _DEBUG
		return false;
	}

	return true;
}
