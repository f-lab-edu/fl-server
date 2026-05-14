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
	bool SendMsg(const UINT32 sessionIndex_, const UINT32 dataSize_, shared_ptr<char[]> pData);

	virtual void OnConnect(const UINT32 clientIndex_) = 0;
	virtual void OnClose(const UINT32 clientIndex_) = 0;
	virtual void OnReceive(const UINT32 clientIndex_, const UINT32 size_, shared_ptr<char[]> pData_) = 0;

private:
	void CreateClient(const UINT32 maxClientCount);
	stClientInfo* GetEmptyClientInfo();
	stClientInfo* GetClientInfo(const UINT32 sessionIndex);

	bool CreateWorkerThread();
	bool CreateAccepterThread();

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
};