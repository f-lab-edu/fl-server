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

bool IOCPServer::SendMsg(const UINT32 sessionIndex_, const UINT32 dataSize_, shared_ptr<char[]> pData)
{
	auto pClient = GetClientInfo(sessionIndex_);
	return pClient->SendMsg(dataSize_, pData);
}

void IOCPServer::CreateClient(const UINT32 maxClientCount)
{
	for (UINT32 i = 0; i < maxClientCount; i++)
	{
		mClientInfos.emplace_back();

		mClientInfos[i].Init(i);
	}
}

stClientInfo* IOCPServer::GetEmptyClientInfo()
{
	for (auto& client : mClientInfos)
	{
		if (client.IsConnected() == false)
		{
			return &client;
		}
	}

	return nullptr;
}

stClientInfo* IOCPServer::GetClientInfo(const UINT32 sessionIndex)
{
	return &mClientInfos[sessionIndex];
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
			printf("socket(%d) connection lost\n", (int)pClientInfo->GetSock());
			CloseSocket(pClientInfo);
			continue;
		}

		auto pOverlappedEx = (stOverlappedEx*)lpOverlapped;

		if (IOOperation::RECV == pOverlappedEx->m_eOperation)
		{
			OnReceive(pClientInfo->GetIndex(), dwIoSize, pClientInfo->RecvBuffer());

			printf("[RECEIVED] bytes : %d , msg : %s\n", dwIoSize, pClientInfo->RecvBuffer());

			pClientInfo->BindRecv();
		}
		else if (IOOperation::SEND == pOverlappedEx->m_eOperation)
		{
			delete[] pOverlappedEx->m_wsaBuf.buf;
			delete pOverlappedEx;
			pClientInfo->SendCompleted(dwIoSize);
		}
		else
		{
			printf("[EXCEPTION] socket(%d)\n", pClientInfo->GetIndex());
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

		auto newSocket = accept(mListenSocket, (SOCKADDR*)&stClientAddr, &nAddrLen);
		if (newSocket == INVALID_SOCKET)
		{
			continue;
		}

		if (pClientInfo->OnConnect(mIOCPHandle, newSocket) == false)
		{
			pClientInfo->Close(true);
			return;
		}

		char clientIP[32] = { 0, };
		inet_ntop(AF_INET, &(stClientAddr.sin_addr), clientIP, 32 - 1);
		printf("Client Connected : IP(%s) SOCKET(%d)\n", clientIP, (int)pClientInfo->GetSock());

		OnConnect(pClientInfo->GetIndex());

		++mClientCnt;
	}
}

void IOCPServer::CloseSocket(stClientInfo* pClientInfo, bool bIsForce)
{
	auto clientIndex = pClientInfo->GetIndex();

	pClientInfo->Close(bIsForce);

	OnClose(clientIndex);
}
