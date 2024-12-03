#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <winsock2.h>
#include <synchapi.h>//WakeConditionVariable()
#include <process.h>
#include <assert.h>

#include "lobbyManager.h"
#include "chessNetworkProtocol.h"
#include "errorLogger.h"
#include "networkWrite.h"

extern CRITICAL_SECTION   g_lobbyMutex;
extern CONDITION_VARIABLE g_gameManagerIsReadyCond;
extern CONDITION_VARIABLE g_lobbyEmptyCond;

#define STRINGIFY(x) #x

typedef struct
{
    SOCKET sock;
    Side side;//white or black pieces
    SOCKADDR_IN addr;
    char ipStr[INET6_ADDRSTRLEN];
}Player;

//put the players back in the lobby and end this thread
static void quitGame(Player* p1, Player* p2)
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

static void forwardMessage(const char* msg, size_t msgSize, const char* msgType, Player* from, Player* to)
{
    printf("forwarding a %s message from %s to %s\n", msgType, from->ipStr, to->ipStr);

    if(networkSendAll(to->sock, msg, msgSize) == SOCKET_ERROR)
    {
        char buff[512] = {0};
        snprintf(buff, sizeof(buff), "write() failed when sending msg from %s to %s\n", from->ipStr, to->ipStr);
        logError(buff, WSAGetLastError());
        closesocket(to->sock);
        quitGame(NULL, from);
    }
}

static void handleInvalidMessageType(Player* from, Player* to)
{
    char const formatStr[] = "invalid message type sent from %s in a game against %s... uh oh";
    char errMsgBuff[256] = {0};
    sprintf_s(errMsgBuff, sizeof(errMsgBuff), formatStr, from->ipStr, to->ipStr);
    logError(errMsgBuff, 0);

    char connectionClosedMsg[OPPONENT_CLOSED_CONNECTION_MSGSIZE] = {
        OPPONENT_CLOSED_CONNECTION_MSGTYPE,
        OPPONENT_CLOSED_CONNECTION_MSGSIZE
    };

    printf("sending a OPPONENT_CLOSED_CONNECTION_MSGTYPE to %s\n", to->ipStr);

    networkSendAll(to->sock, connectionClosedMsg, sizeof(connectionClosedMsg));

    closesocket(from->sock);
    quitGame(NULL, to);
}

//helper func to reduce consumeMessage size
static void handleUnpairMessage(const char* msgBuff, Player* from, Player* to)
{
    //send the same unpair message back to both players
    networkSendAll(from->sock, msgBuff, UNPAIR_MSGSIZE);
    networkSendAll(to->sock, msgBuff, UNPAIR_MSGSIZE);
    quitGame(from, to);
}

static void handleRematchDeclineMessage(const char* msgBuff, Player* from, Player* to)
{
    forwardMessage(msgBuff, REMATCH_DECLINE_MSGSIZE, STRINGIFY(REMATCH_DECLINE_MSGTYPE), from, to);
    quitGame(from, to);
}

static void consumeMessage(const char* msgBuff, size_t* msgBuffCurrentSize, Player* from, Player* to)
{
    size_t const msgSize = msgBuff[1];
    assert(*msgBuffCurrentSize >= msgSize);

    switch(msgBuff[0])
    {
    case UNPAIR_MSGTYPE: {handleUnpairMessage(msgBuff, from, to); break;}
    case MOVE_MSGTYPE: {forwardMessage(msgBuff, MOVE_MSGSIZE, STRINGIFY(MOVE_MSGTYPE), from, to); break;}
    case RESIGN_MSGTYPE: {forwardMessage(msgBuff, RESIGN_MSGSIZE, STRINGIFY(RESIGN_MSGTYPE), from, to); break;}
    case REMATCH_REQUEST_MSGTYPE: {forwardMessage(msgBuff, REMATCH_REQUEST_MSGSIZE, STRINGIFY(REMATCH_REQUEST_MSGTYPE), from, to); break;}
    case REMATCH_ACCEPT_MSGTYPE: {forwardMessage(msgBuff, REMATCH_ACCEPT_MSGSIZE, STRINGIFY(REMATCH_ACCEPT_MSGTYPE), from, to); break;}
    case REMATCH_DECLINE_MSGTYPE: {handleRematchDeclineMessage(msgBuff, from, to); break;}
    case DRAW_ACCEPT_MSGTYPE: {forwardMessage(msgBuff, DRAW_ACCEPT_MSGSIZE, STRINGIFY(DRAW_ACCEPT_MSGSIZE), from, to); break;}
    case DRAW_OFFER_MSGTYPE: {forwardMessage(msgBuff, DRAW_OFFER_MSGSIZE, STRINGIFY(DRAW_OFFER_MSGTYPE), from, to); break;}
    case DRAW_DECLINE_MSGTYPE: {forwardMessage(msgBuff, DRAW_DECLINE_MSGSIZE, STRINGIFY(DRAW_DECLINE_MSGTYPE), from, to); break;}
    default: {handleInvalidMessageType(from, to);}
    }

    //if there are extra bytes in the buffer past this message, move them to the front of the buffer
    if(*msgBuffCurrentSize > msgSize)
    {
        //memmove instead of memcpy since the pointers might overlap
        memmove(msgBuff, msgBuff + msgSize, (*msgBuffCurrentSize) - msgSize);
    }

    *msgBuffCurrentSize -= msgSize;
}

static void sendPairingCompleteMsg(Player* p1, Player* p2)
{
    char buff[PAIR_COMPLETE_MSGSIZE] = {PAIRING_COMPLETE_MSGTYPE, PAIR_COMPLETE_MSGSIZE};
    char whiteOrBlackPieces = (rand() & 1) ? (char)WHITE : (char)BLACK;

    p1->side = whiteOrBlackPieces;
    memcpy(buff + 2, &whiteOrBlackPieces, sizeof(whiteOrBlackPieces));
    int p1SendResult = networkSendAll(p1->sock, buff, sizeof(buff));

    if(p1SendResult == SOCKET_ERROR)
    {
        closesocket(p1->sock);
        quitGame(NULL, p2);
    }

    whiteOrBlackPieces = (whiteOrBlackPieces == WHITE) ? BLACK : WHITE;//swap sides

    p2->side = whiteOrBlackPieces;
    memcpy(buff + 2, &whiteOrBlackPieces, sizeof(whiteOrBlackPieces));
    int p2SendResult = networkSendAll(p2->sock, buff, sizeof(buff));

    if(p2SendResult == SOCKET_ERROR)
    {
        closesocket(p2->sock);
        quitGame(NULL, p1);
    }

    printf("sending PAIRING_COMPLETE_MSG to %s and %s\n", p1->ipStr, p2->ipStr);
}

static void handleSelectErr(Player* p1, Player* p2)
{
    char errMsg[256] = {0};
    snprintf(errMsg, sizeof(errMsg), "select failed in a game between %s and %s", p2->ipStr, p1->ipStr);
    logError(errMsg, WSAGetLastError());
    quitGame(p2, p1);
}

//return true if the whole message has been received.
static bool isMessageReady(const char* msgBuff, size_t currentSize)
{
    size_t expectedMsgSize = msgBuff[1];
    return expectedMsgSize <= currentSize;
}

//handle when recv returns 0
static void handleClosedConnection(const Player* closed, Player* opponent)
{
    char const buff[OPPONENT_CLOSED_CONNECTION_MSGSIZE] = 
    {
        OPPONENT_CLOSED_CONNECTION_MSGTYPE, 
        OPPONENT_CLOSED_CONNECTION_MSGSIZE
    };
    
    if(networkSendAll(opponent->sock, buff, sizeof(buff)) == SOCKET_ERROR)
    {
        closesocket(opponent->sock);
        _endthread();
    }
    
    printf("connection from %s closed. Sending %s to %s\n", closed->ipStr, 
        STRINGIFY(OPPONENT_CLOSED_CONNECTION_MSGTYPE), opponent->ipStr);

    closesocket(closed->sock);
    quitGame(NULL, opponent);
}

//handle when recv returns SOCKET_ERROR
static void handleRecvErr(const Player* errorFrom, Player* opponent)
{
    char errMsg[256] = {0};
    snprintf(errMsg, sizeof(errMsg), "recv failed from %s", errorFrom->ipStr);
    logError(errMsg, WSAGetLastError());
    closesocket(errorFrom->sock);
    quitGame(NULL, opponent);
}

//if this function returns, then recv did not return 0 or SOCKET_ERROR. The size_t returned
//will be the number of bytes retrieved from recv.
static int handleRecvResult(int const recvResult, const Player* receivedFrom, Player* opponent)
{
    if(recvResult == SOCKET_ERROR) { handleRecvErr(receivedFrom, opponent); }
    else if(recvResult == 0) { handleClosedConnection(receivedFrom, opponent); }

    return recvResult;
}

//called when select returns > 0 indicating that there are bytes ready to be read on a socket
static void onSelectReady(Player* bytesReadyPlayer, Player* opponent, char* msgBuff, 
    size_t msgBuffCapacity, size_t* msgBuffCurrentSize)
{
    size_t buffRemainingSize = msgBuffCapacity - *msgBuffCurrentSize;

    int numBytesReceived = handleRecvResult(
        recv(bytesReadyPlayer->sock, msgBuff + *msgBuffCurrentSize, buffRemainingSize, 0),
        bytesReadyPlayer, opponent
    );
    
    *msgBuffCurrentSize += numBytesReceived;

    if(isMessageReady(msgBuff, *msgBuffCurrentSize))
        consumeMessage(msgBuff, msgBuffCurrentSize, bytesReadyPlayer, opponent);
}

//The lobby connections will be coppied into the stack for this thread.
//this means they will be isolated from other game manager threads/lobby thread etc
void __stdcall chessGameThreadStart(void* incommingLobbyConnections)
{
    srand((unsigned)time(NULL));

    //incommingLobbyConnections will point to an array of two LobbyConnection pointers
    //where the first pointer will point to player1, and the second will point to player 2.
    LobbyConnection** lobbyConnections = incommingLobbyConnections;
    Player player1 =
    {
        .sock = lobbyConnections[0]->socket, 
        .side = INVALID,
        .addr = lobbyConnections[0]->addr
    };
    Player player2 =
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

    char player1Buff[2048] = {0};
    size_t player1BuffCurrentSize = 0;

    char player2Buff[2048] = {0};
    size_t player2BuffCurrentSize = 0;

    while(true)
    {
        FD_ZERO(&readSet);
        FD_SET(player1.sock, &readSet);
        FD_SET(player2.sock, &readSet);

        int selectRet = select(0, &readSet, NULL, NULL, &tv);

        if(selectRet == SOCKET_ERROR) { handleSelectErr(&player1, &player2); }
        else if(selectRet > 0)
        {
            if(FD_ISSET(player1.sock, &readSet))
                onSelectReady(&player1, &player2, player1Buff, sizeof(player1Buff), &player1BuffCurrentSize);

            if(FD_ISSET(player2.sock, &readSet))
                onSelectReady(&player2, &player1, player2Buff, sizeof(player2Buff), &player2BuffCurrentSize);
        }
    }
}