#include <WS2tcpip.h>
#include <stdint.h>

//returns -1 if send() fails
int networkSendAll(SOCKET sock, char const* data, size_t const dataSize)
{
    int32_t numBytesSent = 0, numBytesToSend = dataSize;
    while(numBytesSent < dataSize)
    {
        int sendResult = send(sock, data + numBytesSent, numBytesToSend, 0);

        if(sendResult == SOCKET_ERROR) return -1;

        numBytesSent += sendResult;
        numBytesToSend -= sendResult;
    }

    return 0;
}