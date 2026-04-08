@echo off
echo === Starting Lobby Server ===
start "LobbyServer" SERVER.exe

timeout /t 2 >nul

echo === Starting Instance Server #0 (port 5001) ===
start "Instance#0" InstanceServer.exe 0 5001

echo === Starting Instance Server #1 (port 5002) ===
start "Instance#1" InstanceServer.exe 1 5002

echo === Starting Instance Server #2 (port 5003) ===
start "Instance#2" InstanceServer.exe 2 5003

echo === Starting Instance Server #3 (port 5004) ===
start "Instance#2" InstanceServer.exe 3 5004

echo === Starting Instance Server #4 (port 5005) ===
start "Instance#2" InstanceServer.exe 4 5005

echo === All servers started ===
pause