#ifndef CHESSAPPLEVEL_PROTOCOL_H
#define CHESSAPPLEVEL_PROTOCOL_H

#include <stdint.h>

//this header contains the types of messages sent to and from server.
//the first byte of every message sent will be one of the defines here ending in MSGTYPE.
//the defines ending is MSG_SIZE define the size of the whole msg including the first MSGTYPE byte.
//a copy of this header will be present in the client source as well.

typedef int8_t messageType_t;

//the layout of the MOVE_MSGTYPE type of message: 
//|0|1|2|3|4|5|6|
//byte 0 will be the MOVE_MSGTYPE
//byte 1 will be the file (0-7) of the square where the piece is that will be moving
//byte 2 will be the rank (0-7) of the square where the piece is that will be moving
//byte 3 will be the file (0-7) of the square where the piece will be moving to
//byte 4 will be the rank (0-7) of the square where the piece will be moving to
//byte 5 will be the PromoType (enum defined in (client source)moveInfo.h) of the promotion if there is one
//byte 6 will be the MoveInfo (enum defined in (client source)moveInfo.h) of the move that is happening
#define MOVE_MSGTYPE 1
#define MOVE_MSG_SIZE 7

//sent to tell the opponent that you are resigning
#define RESIGN_MSGTYPE 2
#define RESIGN_MSG_SIZE 1

//sent to offer a draw to the opponent
#define DRAW_OFFER_MSGTYPE 3
#define DRAW_OFFER_MSG_SIZE 1

//sent to accept a draw offer
#define DRAW_ACCEPT_MSGTYPE 4
#define DRAW_ACCEPT_MSG_SIZE 1

//sent to decline a draw offer
#define DRAW_DECLINE_MSGTYPE 5
#define DRAW_DECLINE_MSG_SIZE 1

//sent to request a rematch at the end of the game
#define REMATCH_REQUEST_MSGTYPE 6
#define REMATCH_REQUEST_MSG_SIZE 1

//sent to accept a rematch request
#define REMATCH_ACCEPT_MSGTYPE 7
#define REMATCH_ACCEPT_MSG_SIZE 1

//the server is indicating to the client that
//the pairing proccess is complete. after the first
//PAIRING_COMPLETE_MSGTYPE byte, the next byte
//is the side the client is playing as (the Side enum)
typedef enum
#ifdef __cplusplus
struct
#endif
Side {INVALID = 0, WHITE, BLACK} Side;
#define PAIRING_COMPLETE_MSGTYPE 8
#define PAIRING_COMPLETE_MSG_SIZE 2

//sent to request to be paired up with an opponent.
//the 4 bytes after the PAIR_REQUEST_MSGTYPE will be the ip address
//of the opponent that the client would like to pair and play chess with
#define PAIR_REQUEST_MSGTYPE 9
#define PAIR_REQUEST_MSG_SIZE 5

//when the opponent accepts a request to be paired up.
//the 4 bytes after the PAIR_REQUEST_MSGTYPE will be the ip address
//of the player that origonally requested to be paired up with the PAIR_REQUEST_MSGTYPE msg
#define PAIR_ACCEPT_MSGTYPE 10
#define PAIR_ACCEPT_MSG_SIZE 5

//when the opponent declines a request to be paired up
#define PAIR_DECLINE_MSGTYPE 11
#define PAIR_DECLINE_MSG_SIZE 1

//when the opponent doesnt respond in less than PAIR_REQUEST_TIMEOUT_SECS 
//(defined in lobbyConnectionsManager.c for server and chessNetworking.h for client) 
//seconds to a request to pair up and play chess
#define PAIR_NORESPONSE_MSGTYPE 12
#define PAIR_NORESPONSE_MSG_SIZE 1

//sent when someone connects but the server is full. 
//the new connection is closed after this msg is sent
#define SERVER_FULL_MSGTYPE 13
#define SERVER_FULL_MSG_SIZE 1

//when you try to request to play chess 
//with an ip that isnt in the lobby or if you request your own ip
#define IP_NOT_IN_LOBBY_MSGTYPE 14
#define IP_NOT_IN_LOBBY_MSG_SIZE 1

//sent when wanting to disconnect from the
//opponent, but not necessarily the server
#define UNPAIR_MSGTPYE 15
#define UNPAIR_MSG_SIZE 1

//if one of the two chess players lost connections or closed their gamed
#define OPPONENT_CLOSED_CONNECTION_MSGTPYE 16
#define OPPONENT_CLOSED_CONNECTION_MSG_SIZE 1

//sent to decline a REMATCH_REQUEST_MSGTYPE
#define REMATCH_DECLINE_MSGTYPE 17
#define REMATCH_DECLINE_MSG_SIZE 1

//when the client tries to make a pair request 
#define PAIR_REQUEST_TOO_SOON_MSGTYPE 18
#define PAIR_REQUEST_TOO_SOON_MSG_SIZE 1

#endif //CHESSAPPLEVEL_PROTOCOL_H