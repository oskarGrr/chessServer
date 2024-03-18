//This is a multithreaded c TCP socket server meant to manage chess connections for my c++ chess game.
//The client automatically connects to this server upon opening the chess client app.
//The client is then put in a loop inside the function onNewConnection() in a new thread.
//from there the server will wait for them to try to make a connection with another player
//that is also connected to the server. After the two clients are paired messages 
//are free to be transfered back and forth between players

#include <stdio.h>
#include <stdlib.h>
#include "lobbyManager.h"
#include "errorLogger.h"
#include "connectionsAcceptor.h"

#include <winsock2.h>
#include <process.h>
#include <ConsoleApi.h>

int main(void)
{
    BOOL WINAPI signalHandler(_In_ DWORD ctrlSignalType);
    SetConsoleCtrlHandler(signalHandler, TRUE);

    WSADATA wsaData;
    if(WSAStartup(MAKEWORD(2,2), &wsaData))
    {
        logError("winsock initialization failed! ", WSAGetLastError());
        return EXIT_FAILURE;
    }
    
    //The thread responsible for listening to incomming TCP connection attempts.
    //Once a connection is made, the lobby manager thread will be notified
    //and woken up via a condition variable if it is asleep due to no one being on the server.
    HANDLE connectionAccepterThreadHandle = (HANDLE)_beginthread(
        acceptConnectionsThreadStart, ACCEPT_CONNECTIONS_STACKSIZE, NULL);

    //The thread responsible for 
    HANDLE lobbyManagerThreadHandle = (HANDLE)_beginthread(
        lobbyManagerThreadStart, LOBBY_MANAGER_STACKSIZE, NULL);

    //These will be waiting forever.
    //For now the server keeps going until you press ctrl C or close the console.
    WaitForSingleObject(connectionAccepterThreadHandle, INFINITE);
    WaitForSingleObject(lobbyManagerThreadHandle, INFINITE);
    WSACleanup();
    return EXIT_SUCCESS;
}

BOOL WINAPI signalHandler(_In_ DWORD signalType)
{
    switch(signalType)
    {
    case CTRL_C_EVENT: 
        puts("ctrl c event being handled..."); 
        break; 
    case CTRL_CLOSE_EVENT://the console was closed
        break;
    case CTRL_BREAK_EVENT:
        puts("ctrl break/pause event being handled...");
        break;

    default:
        return FALSE;//pass ctrlSignalType to the default handler
    }

    //TODO Send server shutdown message to client.

    ExitProcess(signalType);
    return TRUE;
}