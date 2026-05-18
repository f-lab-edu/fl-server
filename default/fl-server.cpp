#include "pch.h"

#include "EchoServer.h"

int main()
{
	EchoServer server;

	server.Init(MAX_IO_WORKER_THREAD);

	server.BindandListen(SERVER_PORT);

	server.StartServer(MAX_CLIENT);

	printf("");
	while (true)
	{
		string inputCmd;
		::getline(::cin, inputCmd);

		if (inputCmd == "quit")
		{
			break;
		}
	}

	server.End();
	return 0;
}