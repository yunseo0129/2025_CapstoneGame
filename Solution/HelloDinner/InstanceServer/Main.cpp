#include "InstanceServer.h"

int main(int argc, char* argv[])
{
    // 사용법: InstanceServer.exe <instance_id> <port> <lobby_ip>
    // 예시:   InstanceServer.exe 0 5001 127.0.0.1

    int instance_id = 0;
    unsigned short port = INSTANCE_PORT_BASE + 1;
    const char* lobby_ip = "127.0.0.1";

    if (argc >= 2) instance_id = atoi(argv[1]);
    if (argc >= 3) port = static_cast<unsigned short>(atoi(argv[2]));
    if (argc >= 4) lobby_ip = argv[3];

    cout << "========================================" << endl;
    cout << "     INSTANCE SERVER #" << instance_id << endl;
    cout << "========================================" << endl;

    InstanceServer server;

    if (!server.Initialize(instance_id, port, lobby_ip))
        return 1;

    server.Run();
}