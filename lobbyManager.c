#include <assert.h>
#include <stdbool.h> 
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>

#include "gameManager.h"
#include "lobbyManager.h"
#include "chessAppLevelProtocol.h"
#include "errorLogger.h"

//this C file is responsible for the "lobby" thread. the lobby is like a waiting room where
//players are connected to the server, but waiting for a request (or server waiting for them to make request)
//to be paired with another lobby member and play chess, at which point a new thread will
//start to manage the chess game. there is only one single lobby manager thread that sleeps when no one is connected
//and manages all players in the lobby. there might be multiple lobby manager threads in the future if I decide to change it

#define LOBBY_READ_BUFF_SIZE 128

//how long (in seconds) the request to pair up will last before timing out
#define PAIR_REQUEST_TIMEOUT_SECS 10

static LobbyConnection* s_lobbyConnections = NULL;
static size_t s_numOfLobbyConnections = 0;

CRITICAL_SECTION g_lobbyMutex;

//sleep on this if the lobby is empty so the lobby thread doesnt do any busy waiting
CONDITION_VARIABLE g_lobbyEmptyCond;

//sleep on this when calling sendToGameManager()
//until the game manager thread has grabbed the info it needs and the lobby can proceed
CONDITION_VARIABLE g_gameManagerIsReadyCond;
extern bool g_gameConstructionInProgress;//bool for lobby to wait on with the above CV

//get how much room is left in the lobby. lock g_lobbyMutex before calling
size_t getAvailableLobbyRoom()
{
    return LOBBY_CAPACITY - s_numOfLobbyConnections;
}

//lock g_lobbyMutex before calling
bool isLobbyEmpty()
{
    return s_numOfLobbyConnections == 0;
}

static void connectionInfoCtor
(LobbyConnection* const newConn, const int sock, struct sockaddr_in* addr)
{
    newConn->socket = sock;
    newConn->miliSecWaitingOnResoponse = -1;//-1 indicates that the user isnt waiting on a reponse
    memcpy(&newConn->addr, addr, sizeof(*addr));
    InetNtopA(addr->sin_family, &addr->sin_addr, newConn->ipStr, INET6_ADDRSTRLEN);
}

//lock g_lobbyMutex before calling
void lobbyInsert(const int socket, SOCKADDR_IN* addr)
{
    assert(s_lobbyConnections);
    connectionInfoCtor(s_lobbyConnections + s_numOfLobbyConnections, socket, addr);
    ++s_numOfLobbyConnections;
}

//also shrinks the lobby's range if necessary
static void closeLobbyConnection(LobbyConnection* client, size_t* connectionRange, bool shouldCloseSock)
{
    EnterCriticalSection(&g_lobbyMutex);

    if(shouldCloseSock) closesocket(client->socket);
    
    //if the client isnt at the end of the array, then just overwrite the client we are
    //closing with the client at the back of the array, otherwise just decrement the num of lobby connections
    LobbyConnection* clientAtBackOfArray = s_lobbyConnections + (s_numOfLobbyConnections - 1);
    if(client != clientAtBackOfArray) memcpy(client, clientAtBackOfArray, sizeof(*client));

    --s_numOfLobbyConnections;

    if(connectionRange > s_numOfLobbyConnections)
        *connectionRange = s_numOfLobbyConnections;

    LeaveCriticalSection(&g_lobbyMutex);
}

static LobbyConnection* getClientByIP(uint32_t networkByteOrderIp)
{
    //for now since I dont expect more than 2-10 people to be connected at one time,
    //I am just going to loop over every person connected to search for them.
    //in the future I will make some kind of hash function to have O(1) lookups in a table        
    for(int i = 0; i < s_numOfLobbyConnections; ++i)
    {
        //if the requested IP is connected to the server
        if(networkByteOrderIp == s_lobbyConnections[i].addr.sin_addr.s_addr)
            return s_lobbyConnections + i;
    }
    
    return NULL;
}

static void sendLobbyMembersToGameManager(LobbyConnection* client1, 
    LobbyConnection* client2, size_t* currentRange)
{
    //ensure that client1 and 2 pointers are in contiguous memory here..
    LobbyConnection* newChessPair[2] = {client1, client2};

    HANDLE newGameThread = (HANDLE)_beginthread(
        chessGameThreadStart, GAME_MANAGER_STACKSIZE, newChessPair);
    
    EnterCriticalSection(&g_lobbyMutex);
    SleepConditionVariableCS(&g_gameManagerIsReadyCond, &g_lobbyMutex, INFINITE);

    //now that we are awake (the game thread has started and has coppied newChessPair)
    //we can close the lobby connections for client1 and client2

    closeLobbyConnection(client1, currentRange, false);
    closeLobbyConnection(client2, currentRange, false);

    LeaveCriticalSection(&g_lobbyMutex);
}

static void handlePairAccept(const char* msg, LobbyConnection* client, size_t* currentRange)
{
    uint32_t networkByteOrderIp = 0;
    memcpy(&networkByteOrderIp, msg + 1, sizeof(networkByteOrderIp));

    LobbyConnection* opponent = getClientByIP(networkByteOrderIp);
    if(!opponent)//if the person who originally sent the request is no longer in the lobby
    {
        char buff[IP_NOT_IN_LOBBY_MSG_SIZE] = {IP_NOT_IN_LOBBY_MSGTYPE};
        send(client->socket, buff, sizeof(buff), 0);
        printf("sending IP_NOT_IN_LOBBY_MSGTYPE to %s\n", client->ipStr);
    }
    else
    {
        sendLobbyMembersToGameManager(client, opponent, currentRange);
    }
}

static void handlePairRequest(const char* msg, LobbyConnection* client)
{   
    uint32_t nwByteOrderIpOpponent = 0;//the IP of the requested opponent
    memcpy(&nwByteOrderIpOpponent, msg + 1, sizeof(nwByteOrderIpOpponent));
    
    //if already waiting on a response to a pair request message then stop
    //the client from making another request before their timer resets
    if(client->miliSecWaitingOnResoponse > -1)
    {
        char buff[PAIR_REQUEST_TOO_SOON_MSG_SIZE] = {PAIR_REQUEST_TOO_SOON_MSGTYPE};
        send(client->socket, buff, sizeof(buff), 0);
        printf("sending PAIR_REQUEST_TOO_SOON_MSGTYPE to %s", client->ipStr);
        return;
    }

    LobbyConnection* potentialOpponent = getClientByIP(nwByteOrderIpOpponent);
    if(!potentialOpponent || potentialOpponent == client)
    {
        char buff[IP_NOT_IN_LOBBY_MSG_SIZE] = {IP_NOT_IN_LOBBY_MSGTYPE};
        send(client->socket, buff, sizeof(buff), 0);
        printf("sending IP_NOT_IN_LOBBY_MSGTYPE tp %s\n", client->ipStr);
    }
    else
    {
        char buff[PAIR_REQUEST_MSG_SIZE] = {PAIR_REQUEST_MSGTYPE};
        memcpy(buff + 1, &client->addr.sin_addr, sizeof(buff) - 1);
        send(potentialOpponent->socket, buff, sizeof(buff), 0);
        printf("sending PAIR_REQUEST_MSGTYPE to %s\n", potentialOpponent->ipStr);

        //if miliSecWaitingOnRequestResoponse is greater than -1 it signifies that 
        //the lobby member has sent a PAIR_REQUEST_MSGTYPE and is waiting the 
        //PAIR_REQUEST_TIMEOUT_SECS on either a PAIR_ACCEPT_MSGTYPE or PAIR_DECLINE_MSGTYPE
        client->miliSecWaitingOnResoponse = 0;
    }
}

//close connection and return false if msg size doesnt match the type
static bool
confirmMsgSize(size_t msgSize, size_t correctSize, LobbyConnection* client)
{
    if(msgSize == correctSize) return true;
    
    logError("wrong message size for the given type sent from the client uh oh", 0);
    return false;
}

//return false if client was disconnected
static bool handleIncommingLobbyMsg(const char* msg, size_t size,
    LobbyConnection* client, size_t* currentRange)
{
    switch(msg[0])
    {
    case PAIR_REQUEST_MSGTYPE:
    {
        if(!confirmMsgSize(size, PAIR_REQUEST_MSG_SIZE, client)) return false;
        printf("recieved a PAIR_REQUEST_MSGTYPE from %s\n", client->ipStr);
        handlePairRequest(msg, client); break;
    }
    case PAIR_ACCEPT_MSGTYPE:
    {
        if(!confirmMsgSize(size, PAIR_ACCEPT_MSG_SIZE, client)) return false;
        printf("revieced a PAIR_ACCEPT_MSGTYPE from %s\n", client->ipStr);
        handlePairAccept(msg, client, currentRange); break;
    }
    case PAIR_DECLINE_MSGTYPE: break;//TODO make timer for pair requests
    default:
    {
        logError("invalid message type sent from client... uh oh", 0);
        return false;
    }
    }
    
    return true;
}

//if the lobby member has sent a PAIR_REQUEST_MSGTYPE and is waiting PAIR_REQUEST_TIMEOUT_SECS
//for either a PAIR_DECLINE_MSGTYPE or a PAIR_ACCEPT_MSGTYPE then add time to their personal timer
static void addTimeIfNecessary(LobbyConnection* lobbyConn, struct timespec* deltaTime)
{
    //miliSecWaitingOnRequestResoponse will be -1 if they are not waiting
    if(lobbyConn->miliSecWaitingOnResoponse < 0)
        return;

    lobbyConn->miliSecWaitingOnResoponse += deltaTime->tv_sec * 1000;
    lobbyConn->miliSecWaitingOnResoponse += deltaTime->tv_nsec / 1000000;
}

static void checkForTimeout(LobbyConnection* lobbyConn)
{
    if(lobbyConn->miliSecWaitingOnResoponse < PAIR_REQUEST_TIMEOUT_SECS)
        return;

    char buff[PAIR_NORESPONSE_MSG_SIZE] = {PAIR_NORESPONSE_MSGTYPE};
    send(lobbyConn->socket, buff, sizeof(buff), 0);
    printf("sending PAIR_NORESPONSE_MSGTYPE to %s\n", lobbyConn->ipStr);

    //if miliSecWaitingOnRequestResoponse is -1 then it means the lobby member isnt 
    //waiting on a PAIR_DECLINE_MSGTYPE or PAIR_ACCEPT_MSGTYPE
    lobbyConn->miliSecWaitingOnResoponse = -1;
}

//the "lobby" is like a waiting room where players are connected but not paired and playing chess.
//this func is the start of the lobby manager thread (only 1 thread) which will manage the lobby connections.
void __stdcall lobbyManagerThreadStart(void* arg)
{
    assert(!s_lobbyConnections);
    s_lobbyConnections = calloc(LOBBY_CAPACITY, sizeof(LobbyConnection));

    InitializeCriticalSection(&g_lobbyMutex);
    InitializeConditionVariable(&g_lobbyEmptyCond);
    InitializeConditionVariable(&g_gameManagerIsReadyCond);
    
    struct timespec tsStart, tsEnd, deltaTime;
    TIMEVAL selectWaitTime = {.tv_usec = 20};
    fd_set readSockSet;
    FD_ZERO(&readSockSet);

    while(true)
    {
        EnterCriticalSection(&g_lobbyMutex);
        while(s_numOfLobbyConnections == 0)
        {
            puts("The lobby is empty. Lobby thread is going to sleep");
            SleepConditionVariableCS(&g_lobbyEmptyCond, &g_lobbyMutex, INFINITE);
        }

        //capture only the current number of lobby connections. This way
        //the loop below will only work with the lobby members currently connected and not ones
        //that might be inserted while the below loop is doing its thing
        size_t lobbyConnectionRange = s_numOfLobbyConnections;
        LeaveCriticalSection(&g_lobbyMutex);

        (void)_timespec64_get(&tsStart, TIME_UTC);

        for(int i = 0; i < lobbyConnectionRange; ++i)
        {
            FD_ZERO(&readSockSet);
            FD_SET(s_lobbyConnections[i].socket, &readSockSet);
            int selectRet = select(0, &readSockSet, NULL, NULL, &selectWaitTime);
            
            //if the lobby member is waiting on a PAIR_DECLINE_MSGTYPE or PAIR_ACCEPT_MSGTYPE add to their timer
            addTimeIfNecessary(s_lobbyConnections + i, &deltaTime);
            
            //check if the lobby member's timer has reached PAIR_REQUEST_TIMEOUT_SECS
            checkForTimeout(s_lobbyConnections + i);

            //printf("%d\n", s_lobbyConnections[0].miliSecWaitingOnResoponse);

            if(selectRet == SOCKET_ERROR) logError("select() failed with error: ", WSAGetLastError());
            else if(selectRet > 0)
            {   
                char buff[LOBBY_READ_BUFF_SIZE] = {0};
                int recvRet = recv(s_lobbyConnections[i].socket, buff, LOBBY_READ_BUFF_SIZE, 0);
                if(recvRet == 0)
                {
                    closeLobbyConnection(s_lobbyConnections + i, &lobbyConnectionRange, true);
                    --i;
                }
                else if(recvRet == SOCKET_ERROR)
                {
                    logError("recv() error", WSAGetLastError());
                    closeLobbyConnection(s_lobbyConnections + i, &lobbyConnectionRange, true);
                    --i;
                }
                else//recvRet == the number of bytes sent
                {
                    if(!handleIncommingLobbyMsg(buff, recvRet, s_lobbyConnections + i, &lobbyConnectionRange))
                    {
                        closeLobbyConnection(s_lobbyConnections + i, &lobbyConnectionRange, true);
                        --i;
                    }
                }
            }
        }
        
        (void)_timespec64_get(&tsEnd, TIME_UTC);
        deltaTime = (struct timespec)
        {
            .tv_sec  = tsEnd.tv_sec  - tsStart.tv_sec, 
            .tv_nsec = tsEnd.tv_nsec - tsStart.tv_nsec
        };
    }

    free(s_lobbyConnections);
}