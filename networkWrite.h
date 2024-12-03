#pragma once
#include <WS2tcpip.h>

int networkSendAll(SOCKET sock, char const* data, size_t dataSize);

