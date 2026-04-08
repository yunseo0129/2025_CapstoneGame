#include "LobbyServer.h"
#include <iostream>

using namespace std;

int main()
{
    cout << "========================================" << endl;
    cout << "       LOBBY SERVER (Load Balancer)     " << endl;
    cout << "========================================" << endl;

    LobbyServer server;

    if (!server.Initialize())
        return 1;

    server.Run();
}