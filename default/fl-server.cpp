#include "pch.h"

#include "ChatServer.h"

int main()
{
	ChatServer server;

	server.Init(MAX_IO_WORKER_THREAD);

	server.BindandListen(SERVER_PORT);

	server.Run(MAX_CLIENT);

	printf("quit : enter 'quit'");
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