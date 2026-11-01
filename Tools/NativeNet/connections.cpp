// Authored loopback peers only. The same fixture runs through real Event Tick
// inside the integrated Linux client contracts; no credential/auth API is used.
#include "WoW/Source/WowServices/WowConnectionNet.h"
#include "Engine/Source/Base/CDataStore.h"
#include "Engine/Source/Storm/H/storm.h"
#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <poll.h>
#include <stdexcept>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>
namespace {
void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct Fd {
    int value = -1;
    explicit Fd(int fd = -1) : value(fd) {}
    ~Fd() { if (value >= 0) close(value); }
    void Close() { if (value >= 0) close(value); value = -1; }
};
void Nonblocking(int fd) { Require(!fcntl(fd, F_SETFL, O_NONBLOCK), "Fixture nonblocking setup failed"); }
struct Peer {
    Fd listen{socket(AF_INET, SOCK_STREAM, 0)}, stream;
    uint16_t port = 0;
    Peer() {
        Require(listen.value >= 0, "Loopback socket failed");
        sockaddr_in address{}; address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        Require(!bind(listen.value, reinterpret_cast<sockaddr*>(&address), sizeof(address)) && !::listen(listen.value, 8), "Loopback bind/listen failed");
        socklen_t length = sizeof(address);
        Require(!getsockname(listen.value, reinterpret_cast<sockaddr*>(&address), &length), "Loopback port unavailable");
        port = ntohs(address.sin_port); Nonblocking(listen.value);
    }
    void Accept() { if (stream.value < 0) { stream.value = accept(listen.value, nullptr, nullptr); if (stream.value >= 0) Nonblocking(stream.value); } }
    void Put(const std::vector<uint8_t>& bytes) {
        Require(stream.value >= 0 && send(stream.value, bytes.data(), bytes.size(), 0) == ssize_t(bytes.size()), "Fixture write failed");
    }
};
struct Response : WowConnectionResponse {
    std::thread::id owner = std::this_thread::get_id();
    std::vector<int> events;
    std::vector<uint8_t> stream;
    std::vector<std::vector<uint8_t>> packets;
    std::function<void(WowConnection*)> connected, message;
    void Event(int event) { Require(owner == std::this_thread::get_id(), "Off-owner connection callback"); events.push_back(event); }
    void WCConnected(WowConnection* c, WowConnection* inbound, uint32_t, const NETCONNADDR* addr) override {
        Event(1); Require(!inbound && OsNetAddrGetAddress(&addr->peerAddr, nullptr) == htonl(INADDR_LOOPBACK), "Wrong connection peer");
        if (connected) connected(c);
    }
    void WCCantConnect(WowConnection*, uint32_t, const NETCONNADDR*) override { Event(2); }
    void WCDisconnected(WowConnection*, uint32_t, const NETCONNADDR*) override { Event(3); }
    void WCDataReady(WowConnection* c, uint32_t, uint8_t* data, int32_t length) override {
        Event(4); stream.insert(stream.end(), data, data + length); if (message) message(c);
    }
    void WCMessageReady(WowConnection* c, uint32_t, CDataStore* msg) override {
        Event(5); packets.emplace_back(msg->m_data, msg->m_data + msg->m_size); if (message) message(c);
    }
    void WCWriteReady(WowConnection*) override { Event(6); }
};
struct Connection {
    WowConnection* value;
    explicit Connection(Response& response) : value(new WowConnection(&response, nullptr)) {}
    ~Connection() { if (value) { value->SetResponse(nullptr, false); value->Release(); } }
};
void Until(void (*pump)(), std::function<bool()> done, const char* error) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!done()) {
        if (std::chrono::steady_clock::now() >= end) throw std::runtime_error(error);
        pump(); std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
void Connect(void (*pump)(), Connection& c, Response& r, Peer& peer) {
    Require(c.value->Connect("localhost", peer.port, 2000), "Original hostname connect rejected");
    Require(r.events.empty(), "Connect dispatched synchronously");
    Until(pump, [&] { peer.Accept(); return peer.stream.value >= 0 && !r.events.empty(); }, "DNS/connect timed out");
    Require(r.events == std::vector<int>{1}, "Unexpected connect result/order");
    Require((fcntl(c.value->m_sock, F_GETFL) & O_NONBLOCK) && (fcntl(c.value->m_sock, F_GETFD) & FD_CLOEXEC), "Native socket flags missing");
}
void Stream(void (*pump)()) {
    Peer peer; Response response; Connection c(response); c.value->SetType(WOWC_TYPE_STREAM);
    Connect(pump, c, response, peer);
    // Exhaust kernel space without reading; every byte must remain queued once.
    int small = 1024; Require(!setsockopt(c.value->m_sock, SOL_SOCKET, SO_SNDBUF, &small, sizeof(small)), "Fixture send buffer failed");
    std::vector<uint8_t> bytes(512 * 1024);
    for (size_t i = 0; i < bytes.size(); ++i) bytes[i] = uint8_t(i * 73);
    Require(c.value->Send(bytes.data(), bytes.size(), true) == WC_SEND_QUEUED, "Original stream send failed");
    for (unsigned i = 0; i < 25; ++i) pump();
    Require(c.value->m_sendDepth > 0, "Fixture did not exercise queued/backpressured writes");
    std::vector<uint8_t> actual; c.value->m_notifyWriteReady = 1;
    Until(pump, [&] {
        uint8_t chunk[8192]; ssize_t count;
        while ((count = recv(peer.stream.value, chunk, sizeof(chunk), 0)) > 0) actual.insert(actual.end(), chunk, chunk + count);
        return actual.size() == bytes.size() && c.value->m_sendDepth == 0;
    }, "Partial writes failed to drain");
    pump(); Require(actual == bytes && c.value->m_sendBytes == 0 && response.events.back() == 6, "Partial write bytes/order changed");
    peer.Put({0x12}); Until(pump, [&] { return response.stream.size() == 1; }, "Partial stream read missing");
    peer.Put({0x34, 0x56}); Until(pump, [&] { return response.stream.size() == 3; }, "Stream continuation missing");
    Require(response.stream == std::vector<uint8_t>({0x12, 0x34, 0x56}), "Stream read order changed");
    peer.stream.Close(); Until(pump, [&] { return response.events.back() == 3; }, "EOF missing");
    const auto events = response.events; for (unsigned i = 0; i < 3; ++i) pump();
    Require(response.events == events && c.value->m_sock == -1, "Duplicate EOF or open descriptor");
    Peer again; response.events.clear(); Connect(pump, c, response, again);
    c.value->Disconnect(); Require(response.events == std::vector<int>{1}, "Disconnect dispatched synchronously");
    Until(pump, [&] { return response.events.back() == 3; }, "Explicit disconnect missing");
    Require(c.value->m_sendDepth == 0 && c.value->m_readBytes == 0, "Reconnect retained I/O");
}
void Frames(void (*pump)(), bool encrypted, bool large) {
    Peer peer; Response response; Connection c(response); Connect(pump, c, response, peer);
    std::vector<uint8_t> payload(large ? 0x8001 : 7);
    for (size_t i = 0; i < payload.size(); ++i) payload[i] = uint8_t(i * 31 + 9);
    std::vector<uint8_t> wire;
    if (large) wire.push_back(0x80 | (payload.size() >> 16));
    wire.push_back(uint8_t(payload.size() >> 8)); wire.push_back(uint8_t(payload.size()));
    const auto header = wire.size(); wire.insert(wire.end(), payload.begin(), payload.end());
    SARC4Key inbound{}, outbound{};
    if (encrypted) {
        uint8_t key[40]; for (unsigned i = 0; i < sizeof(key); ++i) key[i] = uint8_t(i + 1);
        c.value->m_uint375 = 4; c.value->m_uint376 = 2;
        c.value->SetEncryptionKey(key, sizeof(key), 0, nullptr, 0); c.value->SetEncryption(true);
        inbound = c.value->m_receiveKey; outbound = c.value->m_sendKey;
        SARC4ProcessBuffer(wire.data(), header + 2, &inbound, &inbound);
    }
    for (size_t i = 0; i < header + 2; ++i) {
        peer.Put({wire[i]}); pump();
        Require(response.packets.empty(), "Incomplete frame dispatched");
    }
    peer.Put(std::vector<uint8_t>(wire.begin() + header + 2, wire.end()));
    Until(pump, [&] { return response.packets.size() == 1; }, "Fragmented frame not dispatched");
    Require(response.packets.front() == payload, "Framing/decryption changed payload");
    CDataStore packet; packet.PutData(payload.data(), payload.size()); packet.Finalize();
    Require(c.value->SendPacket(&packet, 0) == WC_SEND_QUEUED, "Framed send rejected");
    std::vector<uint8_t> received;
    Until(pump, [&] {
        uint8_t chunk[8192]; ssize_t count;
        while ((count = recv(peer.stream.value, chunk, sizeof(chunk), 0)) > 0) received.insert(received.end(), chunk, chunk + count);
        return received.size() == header + payload.size();
    }, "Framed send did not drain");
    std::vector<uint8_t> expected(received.size());
    std::copy(wire.begin(), wire.begin() + header, expected.begin());
    // Reconstruct independent plaintext length; inbound opcode width differs.
    if (large) expected[0] = uint8_t(0x80 | (payload.size() >> 16));
    expected[header - 2] = uint8_t(payload.size() >> 8); expected[header - 1] = uint8_t(payload.size());
    std::copy(payload.begin(), payload.end(), expected.begin() + header);
    if (encrypted) SARC4ProcessBuffer(expected.data(), header + 4, &outbound, &outbound);
    Require(received == expected, "12340 framed send/encrypted opcode width changed");
    // Coalesced frames: exactly once, in order, before EOF.
    if (!encrypted) {
        peer.Put({0, 1, 0xaa, 0, 2, 0xbb, 0xcc}); peer.stream.Close();
        Until(pump, [&] { return response.events.back() == 3; }, "Coalesced/EOF missing");
        Require(response.packets.size() == 3 && response.packets[1] == std::vector<uint8_t>{0xaa} && response.packets[2] == std::vector<uint8_t>({0xbb, 0xcc}), "Coalesced frames lost/reordered");
    }
}
void Lifecycle(void (*pump)()) {
    Response response; Connection c(response);
    Require(!c.value->Connect("localhost:65536", 1000) && !c.value->Connect("localhost:12x", 1000) && !c.value->Connect("localhost", 0, 1000), "Malformed endpoint accepted");
    Require(c.value->Connect("localhost", 3724, 0), "Timeout admission failed");
    pump(); Require(response.events == std::vector<int>{2} && c.value->m_sock == -1, "Deadline did not include DNS");
    response.events.clear(); Require(c.value->Connect("localhost", 3724, 1000), "Cancel admission failed");
    c.value->Disconnect(); pump(); Require(response.events == std::vector<int>{3}, "Pending DNS cancellation changed");
    response.events.clear();
    // IPv6 cannot resolve under the original IPv4 contract; no remote DNS.
    Require(c.value->Connect("::1", 3724, 2000), "DNS failure admission failed");
    Until(pump, [&] { return !response.events.empty(); }, "DNS error missing");
    Require(response.events == std::vector<int>{2}, "DNS failure callback changed");
    Peer closed; const auto port = closed.port; closed.listen.Close(); response.events.clear();
    Require(c.value->Connect("127.0.0.1", port, 2000), "Refusal admission failed");
    Until(pump, [&] { return !response.events.empty(); }, "Connection refusal missing");
    Require(response.events == std::vector<int>{2}, "Socket error callback changed");
    bool foreign = false;
    std::thread thread([&] { try { c.value->Disconnect(); } catch (const std::logic_error&) { foreign = true; } });
    thread.join(); Require(foreign, "Off-owner connection call admitted");
    Peer peer; response.events.clear();
    response.connected = [&](WowConnection* value) {
        bool rejected = false; try { WowConnection::s_network->Poll(); } catch (const std::logic_error&) { rejected = true; }
        Require(rejected, "Nested poll admitted");
        rejected = false; try { WowConnection::DestroyOsNet(); } catch (const std::logic_error&) { rejected = true; }
        Require(rejected, "Callback shutdown admitted");
        value->SetResponse(nullptr, true); value->Disconnect();
    };
    Connect(pump, c, response, peer); pump();
    Require(response.events == std::vector<int>{1} && !c.value->m_response, "Deferred null response detach failed");
    // Self-release and replacement response inside a callback retain a pin.
    Response self; auto* released = new WowConnection(&self, nullptr);
    self.connected = [&](WowConnection* value) { value->SetResponse(nullptr, false); value->Release(); released = nullptr; };
    Require(released->Connect("127.0.0.1", peer.port, 2000), "Self-release admission failed");
    Until(pump, [&] { return !released; }, "Self-release callback missing");
}
}
void CheckNativeConnections(void (*pump)()) {
    NETADDR addr{}; uint16_t port = 0; OsNetAddrMake(htonl(0x80abcdef), 54321, &addr);
    Require(OsNetAddrGetAddress(&addr, &port) == htonl(0x80abcdef) && port == 54321, "Native address width/order changed");
    Stream(pump);
    for (bool encrypted : {false, true}) for (bool large : {false, true}) Frames(pump, encrypted, large);
    Lifecycle(pump);
    std::cout << "original-connections=passed DNS stream frames encrypted-headers partial-IO lifecycle owner-callbacks\n";
}
#ifndef WOWDECOMP_CONNECTION_FIXTURE_EMBEDDED
int main() {
    StormRtlInitialize();
    try {
        for (unsigned cycle = 0; cycle < 3; ++cycle) {
            WowConnection::InitOsNet(nullptr, nullptr, 1, false);
            CheckNativeConnections(+[] { WowConnection::s_network->Poll(); });
            // Stop cancels pending DNS without callbacks; late results have no
            // connection pointer and are joined before the owner is destroyed.
            Response pending; Connection c(pending);
            c.value->Connect("localhost", 3724, 2000);
            WowConnection::DestroyOsNet();
            Require(pending.events.empty() && c.value->m_sock == -1 && c.value->m_connState == WOWC_DISCONNECTED, "Shutdown retained dispatch/socket");
            Require(!c.value->Connect("localhost", 3724, 2000), "Retired connection admitted after shutdown");
        }
        StormRtlDestroy(); return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; WowConnection::DestroyOsNet(); StormRtlDestroy(); return 1;
    }
}
#endif
