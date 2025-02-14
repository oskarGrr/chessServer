#ifndef LOBBY_MANAGER_H
#define LOBBY_MANAGER_H

#include <stdbool.h>
#include <ws2tcpip.h>
#include <time.h>
#include <stdint.h>

#define LOBBY_READ_BUFF_SIZE 128

typedef struct
{
    SOCKET socket;
    struct sockaddr_in addr;
    char ipStr[INET6_ADDRSTRLEN];

    //Everyone connected and in the lobby has a unique identifier (0 - RAND_MAX).
    //In the future this identifier will be used as a key in some type 
    //of associative data structure, like a hash table.
    uint32_t uniqueID;

    //everyone needs their own message buffer
    char msgBuff[LOBBY_READ_BUFF_SIZE];
    size_t msgBuffCurrentSize;

}LobbyConnection;

//the size of the stack used by the lobby manager thread in bytes
#define LOBBY_MANAGER_STACKSIZE 64000

#define LOBBY_CAPACITY 50

//the lobby is like a waiting room where players are connected but not paired and playing chess.
//this func is the start of the lobby manager thread (only 1 thread for now maybe more later)
//which will manage the "lobby" connections.
void __stdcall lobbyManagerThreadStart(void*);

//insert a new connected client into the lobby. 
void lobbyInsert(SOCKET socketFd, SOCKADDR_IN* addr);

//Get how much room is left in the lobby. 
size_t getAvailableLobbyRoom(void);

bool isLobbyEmpty(void);

#endif //LOBBY_MANAGER_H