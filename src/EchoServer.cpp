#include "pch.h"

#include "EchoServer.h"

void EchoServer::OnConnect(const UINT32 clientIndex_)
{
	printf("[OnConnect] Client: Index(%d)\n", clientIndex_);
}

void EchoServer::OnClose(const UINT32 clientIndex_)
{
	printf("[OnClose] Client: Index(%d)\n", clientIndex_);
}

void EchoServer::OnReceive(const UINT32 clientIndex_, const UINT32 size_, char* pData_)
{
	printf("[OnReceive] Client: Index(%d), dataSize(%d)\n", clientIndex_, size_);
}
