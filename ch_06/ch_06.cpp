#include "EchoServer.h"

#include <string>
#include <iostream>

const UINT16 SERVER_PORT = 11021;
const UINT16 MAX_CLIENT = 100;

int main()
{
	EchoServer server;

	server.InitSocket();

	server.BindAndListen(SERVER_PORT);

	server.Run(MAX_CLIENT);

	std::cout << "Press AnyKey to QUIT " << std::endl;

	while (true)
	{
		std::string inputCmd;
		std::getline(std::cin, inputCmd);

		if (inputCmd == "quit")
		{
			break;
		}
	}

	return 0;
}
