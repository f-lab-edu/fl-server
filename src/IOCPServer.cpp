#include "pch.h"

#include "IOCPServer.h"

IOCPServer::IOCPServer()
{
}

IOCPServer::~IOCPServer()
{
	WSACleanup();
}

bool IOCPServer::InitSocket()
{
	WSADATA wsaData;

	int nRet = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (nRet != 0)
	{
		printf("[ERROR] WSAStartup() failed : %d\n", WSAGetLastError());
		return false;
	}

	// TCP, Create Overlapped I/O Socket
	mListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);

	if (mListenSocket == INVALID_SOCKET)
	{
		printf("[ERROR] socket() failed : %d\n", WSAGetLastError());
		return false;
	}

	printf("init complete\n");
	return true;
}

bool IOCPServer::BindandListen(int nBindPort)
{
	SOCKADDR_IN stServerAddr;
	stServerAddr.sin_family = AF_INET;
	stServerAddr.sin_port = htons(nBindPort);
	stServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	int nRet = ::bind(mListenSocket, (SOCKADDR*)&stServerAddr, sizeof(SOCKADDR_IN));
	if (nRet != 0)
	{
		printf("[ERROR] bind() failed : %d\n", WSAGetLastError());
		return false;
	}

	nRet = listen(mListenSocket, 5);
	if (nRet != 0)
	{
		printf("[ERROR] listen() failed : %d\n", WSAGetLastError());
		return false;
	}

	printf("server register complete\n");
	return true;
}

bool IOCPServer::StartServer(const UINT32 maxClientCount)
{
	CreateClient(maxClientCount);

	mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, MAX_WORKERTHREAD);
	if (mIOCPHandle == NULL)
	{
		printf("[ERROR] CreateIoCompletionport() failed : %d\n", GetLastError());
		return false;
	}

	bool bRet = CreateWorkerThread();
	if (bRet == false)
	{
		return false;
	}

	bRet = CreateAccepterThread();
	if (bRet == false)
	{
		return false;
	}

	printf("server start\n");
	return true;
}

void IOCPServer::DestroyThread()
{
	mIsWorkerRun = false;
	CloseHandle(mIOCPHandle);

	for (auto& th : mIOWorkerThreads)
	{
		if (th.joinable())
		{
			th.join();
		}
	}

	mIsAccepterRun = false;
	closesocket(mListenSocket);

	if (mAccepterThread.joinable())
	{
		mAccepterThread.join();
	}
}

void IOCPServer::CreateClient(const UINT32 maxClientCount)
{
	for (UINT32 i = 0; i < maxClientCount; i++)
	{
		mClientInfos.emplace_back();

		mClientInfos[i].mIndex = i;
	}
}

bool IOCPServer::CreateWorkerThread()
{
	unsigned int uiThreadId = 0;
	for (int i = 0; i < MAX_WORKERTHREAD; i++)
	{
		mIOWorkerThreads.emplace_back([this]() { WorkerThread(); });
	}

	printf("WorkerThread start\n");
	return true;
}

bool IOCPServer::CreateAccepterThread()
{
	mAccepterThread = thread([this]() { AccepterThread(); });

	printf("AccepterThread start");
	return true;
}

stClientInfo* IOCPServer::GetEmptyClientInfo()
{
	for (auto& client : mClientInfos)
	{
		if (INVALID_SOCKET == client.m_socketClient)
		{
			return &client;
		}
	}

	return nullptr;
}

bool IOCPServer::BindIOCompletionPort(stClientInfo* pClientInfo)
{
	auto hIOCP = CreateIoCompletionPort((HANDLE)pClientInfo->m_socketClient,
		mIOCPHandle,
		(ULONG_PTR)(pClientInfo),
		0);

	if (hIOCP == NULL || hIOCP != mIOCPHandle)
	{
		printf("[ERROR] CreateIoCompletionPort() failed : %d\n", GetLastError());
		return false;
	}

	return true;
}

bool IOCPServer::BindRecv(stClientInfo* pClientInfo)
{
	DWORD dwFlag = { 0 };
	DWORD dwRecvNumBytes = { 0 };

	pClientInfo->m_stRecvOverlappedEx.m_wsaBuf.len = MAX_SOCKBUF;
	pClientInfo->m_stRecvOverlappedEx.m_wsaBuf.buf = pClientInfo->mRecvBuf;
	pClientInfo->m_stRecvOverlappedEx.m_eOperation = IOOperation::RECV;

	int nRet = WSARecv(pClientInfo->m_socketClient,
		&(pClientInfo->m_stRecvOverlappedEx.m_wsaBuf),
		1,
		&dwRecvNumBytes,
		&dwFlag,
		(LPWSAOVERLAPPED) & (pClientInfo->m_stRecvOverlappedEx),
		NULL);

	if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
	{
		printf("[ERROR] WSARecv() failed : %d\n", WSAGetLastError());
		return false;
	}

	return true;
}

bool IOCPServer::SendMsg(stClientInfo* pClientInfo, char* pMsg, int nLen)
{
	DWORD dwRecvNumBytes = { 0 };

	CopyMemory(pClientInfo->mSendBuf, pMsg, nLen);
	pClientInfo->mSendBuf[nLen] = '\0';

	int nRet = WSASend(pClientInfo->m_socketClient,
		&(pClientInfo->m_stSendOverlappedEx.m_wsaBuf),
		1,
		&dwRecvNumBytes,
		0,
		(LPWSAOVERLAPPED) & (pClientInfo->m_stSendOverlappedEx),
		NULL);

	if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
	{
		printf("[ERROR] WSASend() failed : %d\n", WSAGetLastError());
		return false;
	}

	return true;
}

void IOCPServer::WorkerThread()
{
	stClientInfo* pClientInfo = nullptr;
	BOOL bSuccess = TRUE;
	DWORD dwIoSize = { 0 };
	LPOVERLAPPED lpOverlapped = NULL;

	while (mIsWorkerRun)
	{
		bSuccess = GetQueuedCompletionStatus(mIOCPHandle,
			&dwIoSize,
			(PULONG_PTR)&pClientInfo,
			&lpOverlapped,
			INFINITE);

		if (bSuccess == TRUE && dwIoSize == 0 && lpOverlapped == NULL)
		{
			mIsWorkerRun = false;
			continue;
		}

		if (lpOverlapped == NULL)
		{
			continue;
		}

		if (bSuccess == FALSE || (dwIoSize == 0 && bSuccess == TRUE))
		{
			printf("socket(%d) connection lost\n", (int)pClientInfo->m_socketClient);
			CloseSocket(pClientInfo);
			continue;
		}

		auto pOverlappedEx = (stOverlappedEx*)lpOverlapped;

		if (IOOperation::RECV == pOverlappedEx->m_eOperation)
		{
			OnReceive(pClientInfo->mIndex, dwIoSize, pClientInfo->mRecvBuf);

			printf("[RECEIVED] bytes : %d , msg : %s\n", dwIoSize, pClientInfo->mRecvBuf);

			//echo
			//task

			BindRecv(pClientInfo);
		}
		else if (IOOperation::SEND == pOverlappedEx->m_eOperation)
		{
			printf("[SEND] bytes : %d , msg : %s\n", dwIoSize, pClientInfo->mSendBuf);
		}
		else
		{
			printf("[EXCEPTION] socket(%d)\n", (int)pClientInfo->m_socketClient);
		}
	}
}

void IOCPServer::AccepterThread()
{
	SOCKADDR_IN stClientAddr;
	int nAddrLen = sizeof(SOCKADDR_IN);

	while (mIsAccepterRun)
	{
		stClientInfo* pClientInfo = GetEmptyClientInfo();
		if (pClientInfo == NULL)
		{
			printf("[ERROR] Client Full\n");
			return;
		}

		pClientInfo->m_socketClient = accept(mListenSocket, (SOCKADDR*)&stClientAddr, &nAddrLen);
		if (pClientInfo->m_socketClient == INVALID_SOCKET)
		{
			continue;
		}

		bool bRet = BindIOCompletionPort(pClientInfo);
		if (bRet == false)
		{
			return;
		}

		bRet = BindRecv(pClientInfo);
		if (bRet == false)
		{
			return;
		}

		char clientIP[32] = { 0, };
		inet_ntop(AF_INET, &(stClientAddr.sin_addr), clientIP, 32 - 1);
		printf("Client Connected : IP(%s) SOCKET(%d)\n", clientIP, (int)pClientInfo->m_socketClient);

		OnConnect(pClientInfo->mIndex);

		++mClientCnt;
	}
}

void IOCPServer::CloseSocket(stClientInfo* pClientInfo, bool bIsForce)
{
	auto clientIndex = pClientInfo->mIndex;

	linger stLinger = { 0, 0 }; // SO_DONTLINGER

	// bIsForce == true :: SO_LINGER, timeout = 0 // force shutdown, warning : data lost
	if (bIsForce == true)
	{
		stLinger.l_onoff = 1;
	}

	shutdown(pClientInfo->m_socketClient, SD_BOTH);

	setsockopt(pClientInfo->m_socketClient, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof(stLinger));

	closesocket(pClientInfo->m_socketClient);

	pClientInfo->m_socketClient = INVALID_SOCKET;

	OnClose(clientIndex);
}
