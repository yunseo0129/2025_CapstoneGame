#include "IOCPServer.h"

int main()
{
	IOCPServer server;

	if (!server.Initialize())
		return 1;

	server.Run();

	return 0;
}