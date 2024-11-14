#ifndef CHESS_NETWORK_PROTOCOL_H
#define CHESS_NETWORK_PROTOCOL_H

#include <stdint.h>

//This .h file defines the types of messages (and their sizes in bytes)
//that can be sent to and from the server. A copy of this file will be present in
//the server, and client source code. Every message will have a header that will consist of two bytes.
//The first byte will be the MessageType enum defined below.
//The second byte will be the MessageSize enum defined below.

//Sent in the PAIRING_COMPLETE_MSGYPE message (see the MessageType enum below).
//It is defined here since it is used in the client and the server.
typedef enum
#ifdef __cplusplus
struct
#endif
Side
#ifdef __cplusplus
 : uint8_t
#endif
{
    INVALID = 0, WHITE, BLACK
}Side;

//This MessageType enum (1 byte) will be the first byte of every message.
//The next enum below this one (MessageSize) will be the second byte of every message,
//and will signify the size in bytes of the whole message including the two byte header.
typedef enum
#ifdef __cplusplus
struct
#endif
MessageType
#ifdef __cplusplus
 : uint8_t
#endif
{
    //(client to server and server to client)
    //The layout of the MOVE_MSGTYPE type of message (class ChessMove defined in move.h client code):
    // |0|1|2|3|4|5|6|7|8|9|
    //byte 0 will be the MOVE_MSGTYPE  <--- header bytes
    //byte 1 will be the MOVE_MSGSIZE  <---
    //
    //byte 2 will be the file (0-7) the piece if moving from   <--- source square
    //byte 3 will be the rank (0-7) the piece if moving from   <---
    // 
    //byte 4 will be the file (0-7) the piece if moving to   <--- destination square
    //byte 5 will be the rank (0-7) the piece if moving to   <---
    // 
    //byte 6 will be the PromoType (enum defined in (client source)moveInfo.h) of the promotion if there is one
    //byte 7 will be the MoveInfo (enum defined in (client source)moveInfo.h)
    //byte 8 will be the ChessMove::rightsToRevoke as an unsigned char
    //byte 9 will be the ChessMove::wasCapture bool
    //
    //The reason why enum ChessMove::PromoTypes and enum ChessMove::MoveTypes are only defined in the client source is
    //because they are only used as that type there (in the client source). Those bytes are not cast to/de-serialized to
    //their enum types on the server. This message is simply forwarded along from one player/client to the other durring a chess game.
    MOVE_MSGTYPE,

    //(client to server and server to client)
    //Sent to tell your opponent that you are resigning.
    RESIGN_MSGTYPE,

    //(client to server and server to client)
    //Sent to offer a draw to the opponent.
    DRAW_OFFER_MSGTYPE,

    //(client to server and server to client)
    //Sent to accept a draw offer.
    DRAW_ACCEPT_MSGTYPE,

    //(client to server and server to client)
    //Sent to decline a draw offer.
    DRAW_DECLINE_MSGTYPE,

    //(client to server and server to client)
    //Sent to request a rematch at the end of a chess game.
    REMATCH_REQUEST_MSGTYPE,

    //(client to server and server to client)
    //Sent to accept a rematch request.
    REMATCH_ACCEPT_MSGTYPE,

    //(from server to client only)
    //The server is indicating to the client that
    //the pairing proccess is complete. After the first two header bytes, the next byte
    //is the side the client is playing as (the Side enum defined at the top of this file).
    PAIRING_COMPLETE_MSGTYPE,

    //Sent in order to (client to server and server to client)
    //request to pair up and play chess with a potential opponent.
    //The 4 bytes after the first two header bytes will be a network byte order 
    //uint32_t unique identifier (kind of like a friend code).
    //When this message is being sent from client to server,
    //the ID is the ID of the person you wish to play against.
    //When this message is being sent from server to client, 
    //the ID is the ID of the person who sent the pair request to the server origonally.
    PAIR_REQUEST_MSGTYPE,

    //Sent to the server from the client, when the client accepts a PAIR_REQUEST_MSGTYPE.
    //The 4 bytes after the first two header bytes will be a network byte order uint32_t unique identifier
    //of the person who orrigonally sent the PAIR_REQUEST_MSGTYPE.
    PAIR_ACCEPT_MSGTYPE,

    //When the opponent declines a request to be paired up.
    //The 4 bytes after the first two header bytes will be a network byte order uint32_t unique identifier.
    //When this message is being sent from client to server, the ID will be the ID of the player
    //who you want to send a pair decline message to. When this message is being sent from server to client,
    //the ID will be the ID of the player who sent the pair decline message to the server origonally.
    PAIR_DECLINE_MSGTYPE,

    //Sent from the server to the client when the potential opponent 
    //does not respond in less than PAIR_REQUEST_TIMEOUT_SECS
    //(defined in lobbyConnectionsManager.c for server and chessNetworking.h for client) 
    //seconds to a PAIR_REQUEST_MSGTYPE with either a PAIR_DECLINE_MSGTYPE or a PAIR_ACCEPT_MSGTYPE.
    PAIR_NORESPONSE_MSGTYPE,

    //Sent (from server to client only) 
    //when someone connects but the server is full.
    SERVER_FULL_MSGTYPE,

    //Sent (from server to client only) when a player tries to supply an ID
    //of another player which is not in the lobby (or they supply their own ID).
    //The invalid ID is sent back to the client in this message.
    ID_NOT_IN_LOBBY_MSGTYPE,

    //Sent (from server to client and client to server)
    //when in a chess game, and one of the two players wants to disconnect 
    //from the other and go back into the lobby.
    UNPAIR_MSGTYPE,

    //Sent (from server to client only) to signify that an opponent lost connection/closed their game.
    OPPONENT_CLOSED_CONNECTION_MSGTYPE,

    //Sent (from client to server and server to client) to decline a REMATCH_REQUEST_MSGTYPE
    REMATCH_DECLINE_MSGTYPE,

    //Sent (from server to client only) to tell the player that they are sending pair requests too quickly.
    //You have to wait PAIR_REQUEST_TIMEOUT_SECS
    //(defined in lobbyConnectionsManager.c for server and chessNetworking.h for client)
    //before sending another PAIR_REQUEST_MESSAGE.
    PAIR_REQUEST_TOO_SOON_MSGTYPE,

    //(from server to client only)
    //The 4 bytes after the first two header bytes will be a network byte order uint32_t from the server to the client which represents
    //their unique identifier on the server. It is effectively their "friend code" for pairing up with other players.
    NEW_ID_MSGTYPE

}MessageType;

//The size in bytes of the different types of messages (MessageType enum above).
//There will be the same number of enum values here as in MessageType, since the enum values here
//correspond to the same enum name above in the MessageType enum, but with _MSGSIZE instead of _MSGTYPE appended to the enum name.
//Every message will have a two byte header where the first byte is the MessageType enum,
//and the second byte is this MessageSize enum to signify the size of the whole message (header included).
typedef enum
#ifdef __cplusplus
struct
#endif
MessageSize
#ifdef __cplusplus
 : uint8_t
#endif
{
    MOVE_MSGSIZE = 10,
    RESIGN_MSGSIZE = 2,
    DRAW_OFFER_MSGSIZE = 2,
    DRAW_ACCEPT_MSGSIZE = 2,
    DRAW_DECLINE_MSGSIZE = 2,
    REMATCH_REQUEST_MSGSIZE = 2,
    REMATCH_ACCEPT_MSGSIZE = 2,
    PAIR_REQUEST_MSGSIZE = 6,
    PAIR_ACCEPT_MSGSIZE = 6,
    PAIR_DECLINE_MSGSIZE = 6,
    PAIR_COMPLETE_MSGSIZE = 3,
    PAIR_NORESPONSE_MSGSIZE = 2,
    SERVER_FULL_MSGSIZE = 2,
    ID_NOT_IN_LOBBY_MSGSIZE = 6,
    UNPAIR_MSGSIZE = 2,
    OPPONENT_CLOSED_CONNECTION_MSGSIZE = 2,
    REMATCH_DECLINE_MSGSIZE = 2,
    PAIR_REQUEST_TOO_SOON_MSGSIZE = 2,
    NEW_ID_MSGSIZE = 6

}MessageSize;

#endif //CHESS_NETWORK_PROTOCOL_H