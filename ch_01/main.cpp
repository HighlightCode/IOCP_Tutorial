#include "IOCompletionPort.h"
#include <iostream>

const UINT16 SERVER_PORT = 11021;
const UINT16 MAX_CLIENT = 100;


int main()
{
	IOCompletionPort ioCompletionPort;

	ioCompletionPort.InitSocket();

	ioCompletionPort.bindAndListen(SERVER_PORT);

	ioCompletionPort.StartServer(MAX_CLIENT);

	std::cout << "press any key to send msg \n" << std::endl;
	getchar();

	ioCompletionPort.DestroyThread();
	return 0;
}
