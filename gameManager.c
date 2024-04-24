#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <winsock2.h>
#include <synchapi.h>//WakeConditionVariable()
#include <process.h>

#include "lobbyManager.h"
#include "chessAppLevelProtocol.h"
#include "errorLogger.h"

extern CRITICAL_SECTION   g_lobbyMutex;
extern CONDITION_VARIABLE g_gameManagerIsReadyCond;
extern CONDITION_VARIABLE g_lobbyEmptyCond;

#define STRINGIFY(x) #x

typedef struct
{
    SOCKET sock;
    Side side;
    SOCKADDR_IN addr;
    char ipStr[INET6_ADDRSTRLEN];
}ChessPlayer;

static void putPlayersBackAndEndThread(ChessPlayer* p1, ChessPlayer* p2)
{
    const bool lobbyWasEmpty = isLobbyEmpty();

    if(p1)
    {
        lobbyInsert(p1->sock, &p1->addr);
        printf("putting %s back in the lobby\n", p1->ipStr);
    }
    if(p2)
    {
        lobbyInsert(p2->sock, &p2->addr);
        printf("putting %s back in the lobby\n", p2->ipStr);
    }

    if(lobbyWasEmpty) 
    {
        puts("waking the lobby thread up");
        WakeConditionVariable(&g_lobbyEmptyCond);
    }
    _endthread();
}

static void forwardMessage(const char* msg, size_t msgSize, const char* msgType, 
    const ChessPlayer* from, const ChessPlayer* to)
{
    printf("forwarding a %s message from %s to %s\n", msgType, from->ipStr, to->ipStr);
    send(to->sock, msg, msgSize, 0);
}

static const char* recvMessage(ChessPlayer* from, ChessPlayer* to, char* msgBuff, size_t buffSize)
{
    int recvRet = recv(from->sock, msgBuff, sizeof(buffSize), 0);
    if(recvRet == SOCKET_ERROR)
    {
        char errMsg[256] = {0};
        snprintf(errMsg, sizeof(errMsg), "recv failed in a game between %s and %s",
            from->ipStr, to->ipStr);
        logError(errMsg, WSAGetLastError());
        putPlayersBackAndEndThread(from, to);
    }
    if(recvRet == 0)
    {
        char msgTypeStr[] = STRINGIFY(OPPONENT_CLOSED_CONNECTION_MSGTPYE);
        char buff[OPPONENT_CLOSED_CONNECTION_MSG_SIZE] = {OPPONENT_CLOSED_CONNECTION_MSGTPYE};
        send(to->sock, buff, sizeof(buff), 0);
        printf("connection from %s closed. Sending %s to %s\n", from->ipStr, msgTypeStr, to->ipStr);
        closesocket(from->sock);
        putPlayersBackAndEndThread(NULL, to);
    }
}

static void handleInvalidMessageType(ChessPlayer* from, ChessPlayer* to)
{
    const char formatStr[] = "invalid message type sent from %s in a game against %s... uh oh";
    char errMsgBuff[256] = {0};
    sprintf_s(errMsgBuff, sizeof(errMsgBuff), formatStr, from->ipStr, to->ipStr);
    logError(errMsgBuff, 0);
    closesocket(from->sock);
    putPlayersBackAndEndThread(NULL, to);
}

static void handleMessage(const char* msg, ChessPlayer* from, ChessPlayer* to)
{
    switch(msg[0])
    {
    case UNPAIR_MSGTPYE: {forwardMessage(msg, UNPAIR_MSG_SIZE, STRINGIFY(UNPAIR_MSGTPYE), from, to); break;}
    case MOVE_MSGTYPE: {forwardMessage(msg, MOVE_MSG_SIZE, STRINGIFY(MOVE_MSGTYPE), from, to); break;}
    case RESIGN_MSGTYPE: {forwardMessage(msg, RESIGN_MSG_SIZE, STRINGIFY(RESIGN_MSGTYPE), from, to); break;}
    case REMATCH_REQUEST_MSGTYPE: {forwardMessage(msg, REMATCH_REQUEST_MSG_SIZE, STRINGIFY(REMATCH_REQUEST_MSGTYPE), from, to); break;}
    case REMATCH_ACCEPT_MSGTYPE: {forwardMessage(msg, REMATCH_ACCEPT_MSG_SIZE, STRINGIFY(REMATCH_ACCEPT_MSGTYPE), from, to); break;}
    case REMATCH_DECLINE_MSGTYPE: {forwardMessage(msg, REMATCH_DECLINE_MSG_SIZE, STRINGIFY(REMATCH_DECLINE_MSGTYPE), from, to); break;}
    case DRAW_ACCEPT_MSGTYPE: {forwardMessage(msg, DRAW_ACCEPT_MSG_SIZE, STRINGIFY(DRAW_ACCEPT_MSG_SIZE), from, to); break;}
    case DRAW_OFFER_MSGTYPE: {forwardMessage(msg, DRAW_OFFER_MSG_SIZE, STRINGIFY(DRAW_OFFER_MSGTYPE), from, to); break;}
    case DRAW_DECLINE_MSGTYPE: {forwardMessage(msg, DRAW_DECLINE_MSG_SIZE, STRINGIFY(DRAW_DECLINE_MSGTYPE), from, to); break;}
    default: {handleInvalidMessageType(from, to);}
    }

    if(msg[0] == UNPAIR_MSGTPYE || msg[0] == REMATCH_DECLINE_MSGTYPE)
        putPlayersBackAndEndThread(from, to);
}

static void sendPairingCompleteMsg(ChessPlayer* p1, ChessPlayer* p2)
{
    char buff[PAIRING_COMPLETE_MSG_SIZE] = {PAIRING_COMPLETE_MSGTYPE};
    char whiteOrBlackPieces = (rand() & 1) ? (char)WHITE : (char)BLACK;

    p1->side = whiteOrBlackPieces;
    memcpy(buff + 1, &whiteOrBlackPieces, sizeof(whiteOrBlackPieces));
    send(p1->sock, buff, sizeof(buff), 0);

    whiteOrBlackPieces = (whiteOrBlackPieces == WHITE) ? BLACK : WHITE;//swap sides

    p2->side = whiteOrBlackPieces;
    memcpy(buff + 1, &whiteOrBlackPieces, sizeof(whiteOrBlackPieces));
    send(p2->sock, buff, sizeof(buff), 0);
    printf("sending PAIRING_COMPLETE_MSG to %s and %s\n", p1->ipStr, p2->ipStr);
}

//The lobby connections will be coppied into the stack for this thread.
//this means they will be isolated from other game manager threads/lobby thread etc
void __stdcall chessGameThreadStart(void* incommingLobbyConnections)
{
    //rand() needs to be seeded per thread.
    srand((unsigned)time(NULL));

    //incommingLobbyConnections will point to an array of two LobbyConnection pointers
    //where the first pointer will point to player1, and the second will point to player 2.
    LobbyConnection** lobbyConnections = incommingLobbyConnections;
    ChessPlayer player1 =
    {
        .sock = lobbyConnections[0]->socket, 
        .side = INVALID,
        .addr = lobbyConnections[0]->addr
    };
    ChessPlayer player2 =
    {
        .sock = lobbyConnections[1]->socket, 
        .side = INVALID,
        .addr = lobbyConnections[1]->addr
    };

    //wake the lobby up because it is waiting for this thread to finish copying 
    //lobby connections from the lobby into the ChessPair struct above.
    //this is done to just be sure that this thread has started, has the data it needs,
    //and its ready to start the chess game before the lobby thread proceeds

    WakeConditionVariable(&g_gameManagerIsReadyCond);
    
    InetNtopA(player1.addr.sin_family, &player1.addr.sin_addr, player1.ipStr, sizeof(player1.ipStr));
    InetNtopA(player2.addr.sin_family, &player2.addr.sin_addr, player2.ipStr, sizeof(player2.ipStr));

    sendPairingCompleteMsg(&player1, &player2);

    TIMEVAL tv = {.tv_usec = 10};
    fd_set readSet;

    while(true)
    {
        FD_ZERO(&readSet);
        FD_SET(player1.sock, &readSet);
        FD_SET(player2.sock, &readSet);
        int selectRet = select(0, &readSet, NULL, NULL, &tv);

        if(selectRet == SOCKET_ERROR)
        {
            char errMsg[256];
            snprintf(errMsg, sizeof(errMsg), "select failed in a game between %s and %s",
                player1.ipStr, player2.ipStr);
            logError(errMsg, WSAGetLastError());
            putPlayersBackAndEndThread(&player1, &player2);
            _endthread();
        }
        else if(selectRet > 0)
        {
            if(FD_ISSET(player1.sock, &readSet))
            {
                char messageBuff[128] = {0};
                recvMessage(&player1, &player2, messageBuff, sizeof(messageBuff));
                handleMessage(messageBuff, &player1, &player2);
            }
            if(FD_ISSET(player2.sock, &readSet))
            {
                char messageBuff[128] = {0};
                recvMessage(&player2, &player1, messageBuff, sizeof(messageBuff));
                handleMessage(messageBuff, &player2, &player1);
            }
        }
    }
}