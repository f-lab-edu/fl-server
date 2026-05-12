#pragma once

#pragma comment(lib, "ws2_32")

#include <winsock2.h>
#include <Ws2tcpip.h>

#include <thread>
#include <vector>

#include "ClientInfo.h"

using namespace std;

class IOCPServer
{
public:
	IOCPServer();

	virtual ~IOCPServer();

	bool InitSocket();
	bool BindandListen(int nBindPort);
	bool StartServer(const UINT32 maxClientCount);
	void DestroyThread();

	virtual void OnConnect(const UINT32 clientIndex_) = 0;
	virtual void OnClose(const UINT32 clientIndex_) = 0;
	virtual void OnReceive(const UINT32 clientIndex_, const UINT32 size_, char* pData_) = 0;

private:
	void CreateClient(const UINT32 maxClientCount);
	bool CreateWorkerThread();
	bool CreateAccepterThread();

	stClientInfo* GetEmptyClientInfo();

	bool BindIOCompletionPort(stClientInfo* pClientInfo);

	bool BindRecv(stClientInfo* pClientInfo);
	bool SendMsg(stClientInfo* pClientInfo, char* pMsg, int nLen);

	void WorkerThread();

	void AccepterThread();

	void CloseSocket(stClientInfo* pClientInfo, bool bIsForce = false);
	

private:
	vector<stClientInfo> mClientInfos;

	SOCKET mListenSocket = INVALID_SOCKET;

	int mClientCnt = { 0 };

	vector<thread> mIOWorkerThreads;

	thread mAccepterThread;

	HANDLE mIOCPHandle = INVALID_HANDLE_VALUE;

	bool mIsWorkerRun = true;

	bool mIsAccepterRun = true;

	char mSocketBuf[1024] = { 0, };
};