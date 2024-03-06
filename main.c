//This is a multithreaded c TCP socket server meant to manage chess connections for my c++ chess game.
//The client automatically connects to this server upon opening the chess client app.
//The client is then put in a loop inside the function onNewConnection() in a new thread.
//from there the server will wait for them to try to make a connection with another player
//that is also connected to the server. After the two clients are paired messages 
//are free to be transfered back and forth between players

#include <stdlib.h>
#include "lobbyManager.h"
#include "errorLogger.h"
#include "connectionsAcceptor.h"

#include <winsock2.h>
#include <process.h>

int main(void)
{
    WSADATA wsaData;
    if(WSAStartup(MAKEWORD(2,2), &wsaData))
    {
        logError("winsock initialization failed! ", WSAGetLastError());
        return EXIT_FAILURE;
    }
    
    HANDLE connectionAccepterThreadHandle = (HANDLE)_beginthread(
        acceptConnectionsThreadStart, ACCEPT_CONNECTIONS_STACKSIZE, NULL);

    HANDLE lobbyManagerThreadHandle = (HANDLE)_beginthread(
        lobbyManagerThreadStart, LOBBY_MANAGER_STACKSIZE, NULL);

    //these will be waiting forever (for now the server keeps going until you press ctrl C)
    WaitForSingleObject(connectionAccepterThreadHandle, INFINITE);
    WaitForSingleObject(lobbyManagerThreadHandle, INFINITE);
    WSACleanup();
    return EXIT_SUCCESS;
}