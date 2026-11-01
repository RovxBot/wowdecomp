// Existing NETADDR API, using the host sockaddr_in ABI (not a wire structure).
#include "Engine/Source/Os/OsTcp.h"
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <stdexcept>
uint32_t OsNetAddrGetAddress(const NETADDR* addr, uint16_t* port) {
    sockaddr_in value{}; static_assert(sizeof(value) <= sizeof(*addr));
    std::memcpy(&value, addr, sizeof(value));
    if (port) *port = ntohs(value.sin_port);
    return value.sin_addr.s_addr;
}
void OsNetAddrMake(uint32_t address, uint16_t port, NETADDR* addr) {
    sockaddr_in value{}; value.sin_family = AF_INET; value.sin_port = htons(port); value.sin_addr.s_addr = address;
#if defined(__APPLE__)
    value.sin_len = sizeof(value);
#endif
    std::memset(addr, 0, sizeof(*addr)); std::memcpy(addr, &value, sizeof(value));
}
const char* OsNetAddrToStr(const NETADDR* addr, char* buffer, uint32_t size) {
    uint16_t port; in_addr value{}; value.s_addr = OsNetAddrGetAddress(addr, &port);
    char host[INET_ADDRSTRLEN];
    if (!inet_ntop(AF_INET, &value, host, sizeof(host))) throw std::runtime_error("Invalid native IPv4 address");
    std::snprintf(buffer, size, "%s:%u", host, unsigned(port)); return buffer;
}
