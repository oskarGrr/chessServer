#ifndef LOBBY_MANAGER_H
#define LOBBY_MANAGER_H

#include <stdbool.h>
#include <ws2tcpip.h>
#include <time.h>
#include <stdint.h>

typedef struct
{
    SOCKET socket;
    struct sockaddr_in addr;
    char ipStr[INET6_ADDRSTRLEN];
    int32_t miliSecWaitingOnResoponse;//-1 if not waiting
    uint32_t uniqueID;//stored in host byte order
}LobbyConnection;

//the size of the stack used by the lobby manager thread in bytes
#define LOBBY_MANAGER_STACKSIZE 64000

#define LOBBY_CAPACITY 50

//the lobby is like a waiting room where players are connected but not paired and playing chess.
//this func is the start of the lobby manager thread (only 1 thread for now maybe more later)
//which will manage the "lobby" connections.
void __stdcall lobbyManagerThreadStart(void*);

//insert a new connected client into the lobby. 
void lobbyInsert(const int socketFd, SOCKADDR_IN* addr);

//Get how much room is left in the lobby. 
size_t getAvailableLobbyRoom(void);

bool isLobbyEmpty(void);

#endif //LOBBY_MANAGER_H