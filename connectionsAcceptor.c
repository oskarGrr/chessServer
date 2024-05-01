#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "errorLogger.h"
#include "lobbyManager.h"
#include "chessNetworkProtocol.h"

#define RECV_MESSAGE_BUFSIZE 256
#define PORT 42069

extern CONDITION_VARIABLE g_lobbyEmptyCond;
extern CRITICAL_SECTION   g_lobbyMutex;

//returns a valid listen socket file discriptor
static SOCKET createListenSocket(char const* ip, uint16_t port)
{
    SOCKET listenSockfd = SOCKET_ERROR;
    struct sockaddr_in hints;
    memset(&hints, 0, sizeof(hints));

    hints.sin_family = AF_INET;
    hints.sin_port   = htons(port);
    
    if(ip)
    {
        int ptonRet = InetPtonA(AF_INET, ip, &hints.sin_addr);
        if(ptonRet == 0)
        {
            logError("address given to inet_pton is not a valid dotted decimal ip address", 0);
            exit(EXIT_FAILURE);
        }
        else if(ptonRet < 0)
        {
            logError("inet_pton() failed when trying to make the listen socket", WSAGetLastError());
            exit(EXIT_FAILURE);
        }
    }
    else hints.sin_addr.s_addr = INADDR_ANY;
    
    listenSockfd = socket(PF_INET, SOCK_STREAM, 0);
    if(listenSockfd == SOCKET_ERROR)
    {
        logError("a call to socket() failed when trying to make the listen socket", WSAGetLastError());
        WSACleanup();     
        exit(EXIT_FAILURE);
    }
    
    if(bind(listenSockfd, (SOCKADDR*)&hints, sizeof(hints)) == SOCKET_ERROR)
    {
        logError("a call to bind() failed when trying to make the listen socket", WSAGetLastError());
        closesocket(listenSockfd);
        WSACleanup();
        exit(EXIT_FAILURE);
    }
    
    if(listen(listenSockfd, SOMAXCONN) == SOCKET_ERROR)
    {
        logError("a call to listen() failed when trying to make the listen socket", WSAGetLastError());
        closesocket(listenSockfd);
        WSACleanup();
        exit(EXIT_FAILURE);
    }
    
    return listenSockfd;
}

void printConnection(SOCKADDR_IN* addr)
{
    char buff[INET6_ADDRSTRLEN] = {0};
    InetNtopA(AF_INET, &addr->sin_addr, buff, sizeof(buff));
    const char* currentTime = getCurrentTime();
    printf("%s:%hu connected\nat %s\n\n", buff, addr->sin_port, currentTime);
}

static void acceptNewConnections(SOCKET listenSocket)
{
    int addrlen = (int)sizeof(SOCKADDR_IN);
    puts("server started and is accepting connections...");

    while(true)
    {
        SOCKADDR_IN addrInfo;
        memset(&addrInfo, 0, sizeof(addrInfo));
        SOCKET socketFd = accept(listenSocket, (SOCKADDR*)&addrInfo, &addrlen);
        if(socketFd == SOCKET_ERROR)
        {
            logError("accept() failed ", WSAGetLastError());
            exit(1);
        }

        printConnection(&addrInfo);

        if(getAvailableLobbyRoom() == 0)
        {
            char buff[SERVER_FULL_MSGSIZE] = {SERVER_FULL_MSGTYPE};
            send(socketFd, buff, sizeof(buff), 0);
            closesocket(socketFd);
        }
        else
        {
            bool lobbyWasEmpty = isLobbyEmpty();
            lobbyInsert(socketFd, &addrInfo);
            if(lobbyWasEmpty)
            {
                puts("waking the lobby thread up");
                WakeConditionVariable(&g_lobbyEmptyCond);
            }
        }
    }
}

//this is the only function exposed in the .h meant to be the start of a new pthread.
//there is only meant to be 1 thread spawned from this
void __stdcall acceptConnectionsThreadStart(void* ptr)
{
    srand(time(NULL));
    SOCKET listenSocket = createListenSocket(NULL, PORT);
    acceptNewConnections(listenSocket);
}