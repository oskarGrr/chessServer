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

typedef struct
{
    SOCKET sock;
    Side side;
    SOCKADDR_IN addr;
}ChessPlayer;

static void putPlayersBackAndEndThread(ChessPlayer* p1, ChessPlayer* p2, 
    const char* p1IpStr, const char* p2IpStr)
{
    EnterCriticalSection(&g_lobbyMutex);
    bool lobbyWasEmpty = isLobbyEmpty();
    if(p1) 
    {
        lobbyInsert(p1->sock, &p1->addr);
        printf("putting %s back in the lobby\n", p1IpStr);
    }
    if(p2) lobbyInsert(p2->sock, &p2->addr);
    if(lobbyWasEmpty) 
    {
        WakeConditionVariable(&g_lobbyEmptyCond);
        printf("putting %s back in the lobby\n", p2IpStr);
    }
    LeaveCriticalSection(&g_lobbyMutex);
    _endthread();
}

static bool recvAndHandleMsg(ChessPlayer* from, ChessPlayer* to, 
    const char* fromIpStr, const char *toIpStr)
{
    char recvBuff[64] = {0};
    int recvRet = recv(from->sock, recvBuff, sizeof(recvBuff), 0);
    if(recvRet == SOCKET_ERROR)
    {
        char errMsg[256] = {0};
        snprintf(errMsg, sizeof(errMsg), "recv failed in a game between %s and %s",
            fromIpStr, toIpStr);
        logError(errMsg, WSAGetLastError());
        putPlayersBackAndEndThread(from, to, fromIpStr, toIpStr);
    }
    if(recvRet == 0)
    {
        char buff[OPPONENT_CLOSED_CONNECTION_MSG_SIZE] = {OPPONENT_CLOSED_CONNECTION_MSGTPYE};
        send(to->sock, buff, sizeof(buff), 0);
        printf("connection from %s closed, sending OPPONENT_CLOSED_CONNECTION_MSGTPYE to %s\n", 
            fromIpStr, toIpStr);
        closesocket(from->sock);
        putPlayersBackAndEndThread(NULL, to, fromIpStr, toIpStr);
    }
    
    switch(recvBuff[0])
    {
    case UNPAIR_MSGTPYE:
    {
        send(to->sock, recvBuff, UNPAIR_MSG_SIZE, 0);
        printf("forwarding UNPAIR_MSGTPYE from %s to %s\n", fromIpStr, toIpStr);
        putPlayersBackAndEndThread(from, to, fromIpStr, toIpStr);
        break;
    }
    case MOVE_MSGTYPE:
    {
        send(to->sock, recvBuff, MOVE_MSG_SIZE, 0);
        printf("forwarding a MOVE_MSGTYPE from %s to %s\n", fromIpStr, toIpStr);
        break;
    }
    case RESIGN_MSGTYPE:
    {
        send(to->sock, recvBuff, RESIGN_MSG_SIZE, 0);
        printf("forwarding a RESIGN_MSGTYPE from %s to %s\n", fromIpStr, toIpStr);
        //dont put players back in the lobby yet, because they could still rematch
        break;
    }
    case REMATCH_REQUEST_MSGTYPE:
    {
        send(to->sock, recvBuff, REMATCH_REQUEST_MSG_SIZE, 0);
        printf("forwarding a REMATCH_REQUEST_MSGTYPE from %s to %s\n", fromIpStr, toIpStr);
        break;
    }
    case REMATCH_ACCEPT_MSGTYPE:
    {
        send(to->sock, recvBuff, REMATCH_ACCEPT_MSG_SIZE, 0);
        printf("forwarding a REMATCH_ACCEPT_MSGTYPE from %s to %s\n", fromIpStr, toIpStr);
        break;
    }
    case REMATCH_DECLINE_MSGTYPE:
    {
        send(to->sock, recvBuff, REMATCH_DECLINE_MSG_SIZE, 0);
        printf("forwarding a REMATCH_DECLINE_MSGTYPE from %s to %s\n", fromIpStr, toIpStr);
        putPlayersBackAndEndThread(from, to, fromIpStr, toIpStr);
        break;
    }
    default:
        logError("invalid message type sent uh oh", 0);
        return false;
    }

    return true;
}

//The lobby connections will be coppied into the stack for this thread.
//this means they will be isolated from other game manager threads/lobby thread etc
void __stdcall chessGameThreadStart(void* incommingLobbyConnections)
{
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
    
    //Send the informantion that the pairing process is complete and which color pieces to play as.
    {
        srand((unsigned)time(NULL));
        char buff[PAIRING_COMPLETE_MSG_SIZE] = {PAIRING_COMPLETE_MSGTYPE};
        Side whiteOrBlackPieces = (rand() & 1) ? WHITE : BLACK;

        player1.side = whiteOrBlackPieces;
        memcpy(buff + 1, &whiteOrBlackPieces, sizeof(whiteOrBlackPieces));
        send(player1.sock, buff, sizeof(buff), 0);

        whiteOrBlackPieces = (whiteOrBlackPieces == WHITE) ? BLACK : WHITE;//swap sides

        player2.side = whiteOrBlackPieces;
        memcpy(buff + 1, &whiteOrBlackPieces, sizeof(whiteOrBlackPieces));
        send(player2.sock, buff, sizeof(buff), 0);
    }
    
    char player1IPStr[INET6_ADDRSTRLEN] = {0};
    char player2IPStr[INET6_ADDRSTRLEN] = {0};
    InetNtopA(player1.addr.sin_family, &player1.addr.sin_addr, player1IPStr, sizeof(player1IPStr));
    InetNtopA(player2.addr.sin_family, &player2.addr.sin_addr, player2IPStr, sizeof(player2IPStr));
    printf("sending PAIRING_COMPLETE_MSG to %s and %s\n", player1IPStr, player2IPStr);

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
                player1IPStr, player2IPStr);
            logError(errMsg, WSAGetLastError());
            putPlayersBackAndEndThread(&player1, &player2, player1IPStr, player2IPStr);
            _endthread();
        }
        else if(selectRet > 0)
        {
            if(FD_ISSET(player1.sock, &readSet))
                recvAndHandleMsg(&player1, &player2, player1IPStr, player2IPStr);
            if(FD_ISSET(player2.sock, &readSet))
                recvAndHandleMsg(&player2, &player1, player2IPStr, player1IPStr);
        }
    }
}