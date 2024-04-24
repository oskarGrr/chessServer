#ifndef CHESSAPPLEVEL_PROTOCOL_H
#define CHESSAPPLEVEL_PROTOCOL_H

#include <stdint.h>

//The message type is the 1 byte header of every message to signigy what the following bytes represent.
//Every message type has a corresponding message size next to it in this file. The corresponding message
//size is how big the message will be (including the 1 byte messageType_t header).
typedef uint8_t messageType_t;

//This .h file defines the types of messages that can be sent to and from the server.
//I would define these with an enum class/struct instead of macros, but the server is written in C.
//C does not have scoped enums, and I this header file should be present in the C++ client as well as in the C server.
//I might do something like this in the future for message types and sizes:
//enum
//#ifdef __cplusplus
//struct
//#endif
//messageTypes
//#ifdef __cplusplus
// : messageType_t
//#endif
//{
//    MOVE_MESSAGE,
//    MOVE_MESSAGE_SIZE,
//    PAIR_REQUEST_MESSAGE,
//    PAIR_REQUEST_MESSAGE_SIZE,
//    etc...
//};

//the first byte of every message sent will be one of the defines here ending in MSGTYPE.
//the defines ending is MSG_SIZE define the size of the whole msg including the first MSGTYPE byte.
//a copy of this header will be present in the client source as well.

//The layout of the MOVE_MSGTYPE type of message: 
//|0|1|2|3|4|5|6|
//Byte 0 will be the MOVE_MSGTYPE.
//Byte 1 will be the file (0-7) of the square where the piece is that will be moving.
//Byte 2 will be the rank (0-7) of the square where the piece is that will be moving.
//Byte 3 will be the file (0-7) of the square where the piece will be moving to.
//Byte 4 will be the rank (0-7) of the square where the piece will be moving to.
//Byte 5 will be the PromoType (enum defined in (client source)moveInfo.h) of the promotion if there is one.
//Byte 6 will be the MoveInfo (enum defined in (client source)moveInfo.h) of the move that is happening.
#define MOVE_MSGTYPE 1
#define MOVE_MSG_SIZE 7

//(client to server and server to client)
//Sent to tell your opponent that you are resigning.
#define RESIGN_MSGTYPE 2
#define RESIGN_MSG_SIZE 1

//(client to server and server to client)
//Sent to offer a draw to the opponent.
#define DRAW_OFFER_MSGTYPE 3
#define DRAW_OFFER_MSG_SIZE 1

//(client to server and server to client)
//Sent to accept a draw offer.
#define DRAW_ACCEPT_MSGTYPE 4
#define DRAW_ACCEPT_MSG_SIZE 1

//(client to server and server to client)
//Sent to decline a draw offer.
#define DRAW_DECLINE_MSGTYPE 5
#define DRAW_DECLINE_MSG_SIZE 1

//(client to server and server to client)
//Sent to request a rematch at the end of a chess game.
#define REMATCH_REQUEST_MSGTYPE 6
#define REMATCH_REQUEST_MSG_SIZE 1

//(client to server and server to client)
//Sent to accept a rematch request.
#define REMATCH_ACCEPT_MSGTYPE 7
#define REMATCH_ACCEPT_MSG_SIZE 1

//(from server to client only)
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

//Sent in order to (client to server and server to client)
//request to pair up and play chess with a potential opponent.
//The 4 bytes after the PAIR_REQUEST_MSGTYPE will be a network byte order 
//uint32_t unique identifier (kind of like a friend code).
//When this message is being sent from client to server,
//the ID is the ID of the person you wish to play against.
//When this message is being sent from server to client, 
//the ID is the ID of the person who sent the pair request to the server origonally.
#define PAIR_REQUEST_MSGTYPE 9
#define PAIR_REQUEST_MSG_SIZE 5

//When the opponent accepts a PAIR_REQUEST_MSGTYPE (client to server and server to client).
//The 4 bytes after the PAIR_ACCEPT_MSGTYPE will be a network byte order uint32_t unique identifier.
//When this message is being sent from client to server, the ID will be the ID of the player
//who you want to send a pair accept message to. When this message is being sent from server to client,
//the ID will be the ID of the player who sent the pair accept message to the server origonally.
#define PAIR_ACCEPT_MSGTYPE 10
#define PAIR_ACCEPT_MSG_SIZE 5

//When the opponent declines a request to be paired up.
//The 4 bytes after the PAIR_DECLINE_MSGTYPE will be a network byte order uint32_t unique identifier.
//When this message is being sent from client to server, the ID will be the ID of the player
//who you want to send a pair decline message to. When this message is being sent from server to client,
//the ID will be the ID of the player who sent the pair decline message to the server origonally.
#define PAIR_DECLINE_MSGTYPE 11
#define PAIR_DECLINE_MSG_SIZE 5

//Sent from the server to the client when the potential opponent 
//does not respond in less than PAIR_REQUEST_TIMEOUT_SECS
//(defined in lobbyConnectionsManager.c for server and chessNetworking.h for client) 
//seconds to a PAIR_REQUEST_MSGTYPE with either a PAIR_DECLINE_MSGTYPE or a PAIR_ACCEPT_MSGTYPE.
#define PAIR_NORESPONSE_MSGTYPE 12
#define PAIR_NORESPONSE_MSG_SIZE 1

//Sent (from server to client only) 
//when someone connects but the server is full.
#define SERVER_FULL_MSGTYPE 13
#define SERVER_FULL_MSG_SIZE 1

//Sent (from server to client only) when a player tries to supply an ID
//of another player which is not in the lobby (or they supply their own ID).
#define ID_NOT_IN_LOBBY_MSGTYPE 14
#define ID_NOT_IN_LOBBY_MSG_SIZE 1

//Sent (from server to client and client to server)
//when in a chess game, and one of the two players wants to disconnect 
//from the other and go back into the lobby.
#define UNPAIR_MSGTPYE 15
#define UNPAIR_MSG_SIZE 1

//Sent (from server to client only) to signify that an opponent lost connection/closed their game.
#define OPPONENT_CLOSED_CONNECTION_MSGTPYE 16
#define OPPONENT_CLOSED_CONNECTION_MSG_SIZE 1

//Sent (from client to server and server to client) to decline a REMATCH_REQUEST_MSGTYPE
#define REMATCH_DECLINE_MSGTYPE 17
#define REMATCH_DECLINE_MSG_SIZE 1

//Sent (from server to client only) to tell the player that they are sending pair requests too quickly.
//You have to wait PAIR_REQUEST_TIMEOUT_SECS
//(defined in lobbyConnectionsManager.c for server and chessNetworking.h for client)
//before sending another PAIR_REQUEST_MESSAGE.
#define PAIR_REQUEST_TOO_SOON_MSGTYPE 18
#define PAIR_REQUEST_TOO_SOON_MSG_SIZE 1

//(from server to client only)
//The first byte will be the NEW_ID_MSGTYPE.
//The next 4 bytes will be a network byte order uint32_t from the server to the client which represents
//their unique identifier on the server. It is effectively their "friend code" for pairing up with other players.
#define NEW_ID_MSGTYPE 19
#define NEW_ID_MSG_SIZE 5

#endif //CHESSAPPLEVEL_PROTOCOL_H