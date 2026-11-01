// POSIX adaptation of this checkout's original 12340 WowConnection/Win64 path.
// Shared SENDNODE, ARC4/HMAC, CDataStore and WowConnectionResponse stay original.
// Native runtime objects use host alignment; no Windows address verification is
// claimed for these methods. Authentication/integrity policy is outside this layer.
#include "Connection.h"
#include "WoW/Source/WowServices/WowConnectionNet.h"
#include "Engine/Source/Base/CDataStore.h"
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Storm/H/storm.h"
#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <climits>
#include <cstring>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

namespace {
struct Pin {
    WowConnection& connection;
    explicit Pin(WowConnection& value) : connection(value) { connection.AddRef(); }
    ~Pin() { connection.Release(); }
};
struct Response {
    WowConnection& connection;
    WowConnectionResponse* value;
    explicit Response(WowConnection& c) : connection(c), value(c.m_response) { c.AcquireResponseRef(); }
    ~Response() { connection.ReleaseResponseRef(); }
};
bool Blocked() { return errno == EAGAIN || errno == EWOULDBLOCK; }
bool Configure(int fd) {
    int flags = fcntl(fd, F_GETFL), descriptor = fcntl(fd, F_GETFD);
    if (flags < 0 || descriptor < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) || fcntl(fd, F_SETFD, descriptor | FD_CLOEXEC)) return false;
    const int enabled = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled))) return false;
#if defined(__APPLE__)
    if (setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled))) return false;
#endif
    return true;
}
ssize_t Write(int fd, const uint8_t* data, size_t length) {
    ssize_t sent;
#if defined(MSG_NOSIGNAL)
    do { sent = send(fd, data, length, MSG_NOSIGNAL); } while (sent < 0 && errno == EINTR);
#else
    do { sent = send(fd, data, length, 0); } while (sent < 0 && errno == EINTR);
#endif
    return sent;
}
}
void WowConnection::CheckNativeOwner() const {
    if (!m_native || m_native->owner != std::this_thread::get_id())
        throw std::logic_error("WowConnection requires its Event owner thread");
}
void WowConnection::Init(WowConnectionResponse* response, void (*threadInit)()) {
    if (!s_network) throw std::logic_error("Initialize native connections before creating one");
    s_network->CheckOwner();
    auto native = std::make_unique<Native>(); native->network = s_network;
    m_refCount.m_value = 1; m_sock = m_oldsock = -1; m_connState = WOWC_INITIALIZED;
    m_response = response; m_pendingResponse = nullptr; m_responseRef = 0; m_responseRefThread = 0;
    m_readBuffer = nullptr; m_readBytes = m_readBufferSize = 0;
    m_threadInit = threadInit; m_connectAddress = 0; m_connectPort = 0; m_connectRetryInterval = -1;
    m_peer = {}; m_sendDepth = m_sendBytes = 0; m_maxSendDepth = 100000;
    m_serviceFlags = m_serviceCount = 0; m_event = nullptr; m_type = WOWC_TYPE_MESSAGES;
    m_notifyWriteReady = 0; m_encrypt = false; m_uint375 = m_uint376 = 0;
    std::memset(m_sendKeyInit, 0, sizeof(m_sendKeyInit)); std::memset(m_receiveKeyInit, 0, sizeof(m_receiveKeyInit));
    m_sendKey = {}; m_receiveKey = {};
    s_network->Add(this); m_native = native.release();
}
WowConnection::WowConnection(WowConnectionResponse* response, void (*threadInit)()) { Init(response, threadInit); }
WowConnection::WowConnection(int32_t sock, const NETADDR*, WowConnectionResponse* response) {
    Init(response, nullptr);
    if (!Configure(sock)) { s_network->Remove(this); delete m_native; m_native = nullptr; throw std::runtime_error("Cannot configure inbound native socket"); }
    m_sock = sock; m_connState = WOWC_CONNECTED;
    socklen_t size = sizeof(m_peer.peerAddr); getpeername(sock, reinterpret_cast<sockaddr*>(&m_peer.peerAddr), &size);
    size = sizeof(m_peer.selfAddr); getsockname(sock, reinterpret_cast<sockaddr*>(&m_peer.selfAddr), &size);
}
WowConnection::~WowConnection() {
    CheckNativeOwner();
    if (m_native->network) m_native->network->Remove(this);
    NativeReset();
    if (m_readBuffer) SMemFree(m_readBuffer, __FILE__, __LINE__, 0);
    m_sendKey = {}; m_receiveKey = {};
    std::memset(m_sendKeyInit, 0, sizeof(m_sendKeyInit)); std::memset(m_receiveKeyInit, 0, sizeof(m_receiveKeyInit));
    delete m_native;
}
void WowConnection::AddRef() { CheckNativeOwner(); m_refCount.Increment(); }
int32_t WowConnection::Release() {
    CheckNativeOwner();
    const auto count = m_refCount.Decrement();
    if (!count) delete this;
    return count;
}
void WowConnection::AcquireResponseRef() { CheckNativeOwner(); ++m_responseRef; }
void WowConnection::ReleaseResponseRef() {
    CheckNativeOwner();
    if (!--m_responseRef && m_native->pendingResponse) {
        m_response = m_pendingResponse; m_pendingResponse = nullptr; m_native->pendingResponse = false;
    }
}
void WowConnection::SetResponse(WowConnectionResponse* response, bool deferred) {
    CheckNativeOwner();
    if (deferred && m_responseRef) {
        m_pendingResponse = response; m_native->pendingResponse = true;
    } else {
        m_response = response; m_pendingResponse = nullptr; m_native->pendingResponse = false;
    }
}
int32_t WowConnection::InitOsNet(bool (*filter)(const NETADDR*), void (*threadInit)(), int32_t threads, bool useEngine) {
    if (useEngine || threads != 1) throw std::invalid_argument("Native sockets use the single client Event owner");
    if (s_network) { s_network->CheckOwner(); return 1; }
    s_network = new WowConnectionNet(threads, threadInit); s_verifyAddr = filter;
    return 1;
}
void WowConnection::DestroyOsNet() {
    if (!s_network) return;
    s_network->Stop(); delete s_network; s_network = nullptr; s_verifyAddr = nullptr;
}
void WowConnection::CloseSocket(int32_t sock) {
    CheckNativeOwner();
    // Never retry close after EINTR: another descriptor may reuse that number.
    if (sock >= 0) close(sock);
}
void WowConnection::NativeReset() {
    CheckNativeOwner();
    ++m_native->generation; m_native->lookup.reset(); m_native->ready = m_native->failed = false;
    if (m_sock >= 0) CloseSocket(m_sock);
    m_sock = -1; m_readBytes = 0; FreeSendList(); m_sendDepth = m_sendBytes = 0; m_notifyWriteReady = 0;
}
void WowConnection::NativeNotify(WOW_CONN_STATE result) {
    Pin pin(*this);
    const auto peer = m_peer;
    if (result != WOWC_CONNECTED) NativeReset();
    m_connState = result == WOWC_ERROR ? WOWC_DISCONNECTED : result;
    Response response(*this);
    if (!response.value) return;
    const auto now = OsGetAsyncTimeMs();
    if (result == WOWC_CONNECTED) response.value->WCConnected(this, nullptr, now, &peer);
    else if (result == WOWC_ERROR) response.value->WCCantConnect(this, now, &peer);
    else response.value->WCDisconnected(this, now, &peer);
}
bool WowConnection::Connect(const char* host, uint16_t port, int32_t timeout) {
    CheckNativeOwner();
    if (!m_native->network || !host || !*host || std::strlen(host) > 255 || !port || timeout < -1) return false;
    // Queue lookup before replacing a live connection, so admission/allocation
    // failures leave its descriptor, packets and callbacks intact.
    auto lookup = m_native->network->Resolve(host);
    NativeReset(); m_connectPort = port; m_connectAddress = 0; m_peer = {};
    m_native->lookup = std::move(lookup); m_native->timed = timeout >= 0;
    m_native->deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(std::max(timeout, 0));
    m_connState = WOWC_CONNECTING; return true;
}
bool WowConnection::Connect(const char* address, int32_t timeout) {
    CheckNativeOwner();
    if (!address || std::strlen(address) > 261) return false;
    std::string host(address); const auto colon = host.find(':');
    if (colon == std::string::npos || !colon || colon + 1 == host.size()) return false;
    uint32_t port = 0;
    for (size_t i = colon + 1; i < host.size(); ++i) {
        if (host[i] < '0' || host[i] > '9') return false;
        port = port * 10 + unsigned(host[i] - '0'); if (port > 65535) return false;
    }
    host.resize(colon); return Connect(host.c_str(), uint16_t(port), timeout);
}
bool WowConnection::Connect(uint32_t address, uint16_t port, int32_t timeout) {
    in_addr value{}; value.s_addr = address; char host[INET_ADDRSTRLEN];
    return inet_ntop(AF_INET, &value, host, sizeof(host)) && Connect(host, port, timeout);
}
void WowConnection::StartConnect() {
    CheckNativeOwner();
    sockaddr_in address{}; address.sin_family = AF_INET; address.sin_addr.s_addr = m_connectAddress; address.sin_port = htons(m_connectPort);
    std::memcpy(&m_peer.peerAddr, &address, sizeof(address));
    if (s_verifyAddr && !s_verifyAddr(&m_peer.peerAddr)) { m_native->failed = true; return; }
    m_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (m_sock < 0 || !Configure(m_sock)) { m_native->failed = true; return; }
    const int result = connect(m_sock, reinterpret_cast<sockaddr*>(&address), sizeof(address));
    if (!result) m_native->ready = true;
    else if (errno != EINPROGRESS && errno != EINTR) m_native->failed = true;
}
void WowConnection::CheckConnect() {
    CheckNativeOwner();
    int error = 0; socklen_t size = sizeof(error);
    if (getsockopt(m_sock, SOL_SOCKET, SO_ERROR, &error, &size) || error) { NativeNotify(WOWC_ERROR); return; }
    size = sizeof(m_peer.peerAddr);
    if (getpeername(m_sock, reinterpret_cast<sockaddr*>(&m_peer.peerAddr), &size)) { NativeNotify(WOWC_ERROR); return; }
    size = sizeof(m_peer.selfAddr);
    if (getsockname(m_sock, reinterpret_cast<sockaddr*>(&m_peer.selfAddr), &size)) { NativeNotify(WOWC_ERROR); return; }
    NativeNotify(WOWC_CONNECTED);
}
void WowConnection::Disconnect() {
    CheckNativeOwner();
    if (m_connState == WOWC_CONNECTING || m_connState == WOWC_CONNECTED) {
        ++m_native->generation; m_connState = WOWC_DISCONNECTING;
    }
}
void WowConnection::DoDisconnect() { CheckNativeOwner(); NativeNotify(WOWC_DISCONNECTED); }
void WowConnection::DoExceptions() { CheckNativeOwner(); if (m_connState == WOWC_CONNECTING) CheckConnect(); }
void WowConnection::CheckAccept() { throw std::logic_error("Native client transport does not install a listener"); }
char* WowConnection::PeerAddrToStr(char* buffer, uint32_t size) {
    CheckNativeOwner(); OsNetAddrToStr(&m_peer.peerAddr, buffer, size); return buffer;
}
void WowConnection::NativePoll(short readiness) {
    CheckNativeOwner();
    if (s_lagTestDelayMin || s_lagTestDelayMax) throw std::logic_error("Native lag simulation is not installed");
    if (m_connState == WOWC_DISCONNECTING) { DoDisconnect(); return; }
    if (m_connState == WOWC_CONNECTING) {
        if (m_native->timed && std::chrono::steady_clock::now() >= m_native->deadline) { NativeNotify(WOWC_ERROR); return; }
        if (m_native->lookup) {
            if (!m_native->lookup->done.load(std::memory_order_acquire)) return;
            const auto lookup = std::move(m_native->lookup);
            if (!lookup->success) { NativeNotify(WOWC_ERROR); return; }
            m_connectAddress = lookup->address; StartConnect();
            // Readiness was collected before this descriptor existed.
            readiness = 0;
        }
        if (m_native->failed) { NativeNotify(WOWC_ERROR); return; }
        if (m_native->ready || (readiness & (POLLOUT | POLLERR | POLLHUP | POLLNVAL))) CheckConnect();
        return;
    }
    if (m_connState != WOWC_CONNECTED) return;
    const auto generation = m_native->generation;
    // Preserve WowConnectionNet::Service's write-before-read dispatch. A
    // write callback may cancel/reconnect, so recheck before using read readiness.
    if (readiness & POLLOUT) DoWrites();
    // A bounded previous read can leave a complete frame in user storage
    // after consuming all kernel bytes. Service it without another fd edge.
    if (m_native->generation == generation && m_connState == WOWC_CONNECTED &&
        ((readiness & (POLLIN | POLLHUP | POLLERR | POLLNVAL)) || (m_type == WOWC_TYPE_MESSAGES && m_readBytes))) DoReads();
}
WC_SEND_RESULT WowConnection::SendPacket(CDataStore* msg, int32_t) {
    CheckNativeOwner();
    if (!msg || !msg->m_size || msg->m_size > 0x7fffff || msg->m_read != 0 || m_connState != WOWC_CONNECTED) return WC_SEND_ERROR;
    void* data = nullptr; msg->GetDataInSitu(data, msg->m_size);
    auto* node = NewSendNodePacket(data, int32_t(msg->m_size), false);
    if (m_sendDepth >= m_maxSendDepth || node->m_size > uint32_t(INT_MAX - m_sendBytes)) {
        FreeSendNode(node); Disconnect(); return WC_SEND_ERROR;
    }
    if (m_encrypt) Encrypt(node->m_data, std::min(node->m_size, uint32_t(m_uint375) + node->m_size - node->m_datasize));
    m_sendList.LinkToTail(node); ++m_sendDepth; m_sendBytes += node->m_size; s_sendBytes += msg->m_size;
    return WC_SEND_QUEUED;
}
WC_SEND_RESULT WowConnection::Send(uint8_t* data, int32_t size, bool raw) {
    CheckNativeOwner();
    if (!data || size <= 0 || m_connState != WOWC_CONNECTED) return WC_SEND_ERROR;
    if (m_sendDepth >= m_maxSendDepth || size > INT_MAX - m_sendBytes) { Disconnect(); return WC_SEND_ERROR; }
    auto* node = NewSendNodePacket(data, size, true);
    if (!raw && m_encrypt) Encrypt(node->m_data, std::min(uint32_t(size), uint32_t(m_uint375) + (size - 2 > 0x7fff) + 2));
    m_sendList.LinkToTail(node); ++m_sendDepth; m_sendBytes += node->m_size; s_sendBytes += size;
    return WC_SEND_QUEUED;
}
void WowConnection::DoWrites() {
    CheckNativeOwner(); Pin pin(*this);
    if (m_connState != WOWC_CONNECTED) return;
    for (unsigned budget = 0; budget < 64; ++budget) {
        auto* node = m_sendList.Head(); if (!node) break;
        const auto sent = Write(m_sock, node->m_data + node->m_offset, std::min(node->m_size - node->m_offset, 1024u));
        if (sent < 0 && Blocked()) return;
        if (sent <= 0) { NativeNotify(WOWC_DISCONNECTED); return; }
        node->m_offset += uint32_t(sent);
        if (node->m_offset == node->m_size) {
            m_sendList.UnlinkNode(node); --m_sendDepth; m_sendBytes -= node->m_size; FreeSendNode(node);
        }
    }
    if (!m_sendList.Head() && m_notifyWriteReady) {
        m_notifyWriteReady = 0; Response response(*this);
        if (response.value) response.value->WCWriteReady(this);
    }
}
void WowConnection::DoReads() {
    CheckNativeOwner(); Pin pin(*this);
    if (m_connState != WOWC_CONNECTED) return;
    if (m_type == WOWC_TYPE_STREAM) DoStreamReads(); else DoMessageReads();
}
void WowConnection::DoStreamReads() {
    uint8_t buffer[4096]; const auto generation = m_native->generation;
    for (unsigned budget = 0; budget < 16; ++budget) {
        ssize_t count;
        do { count = recv(m_sock, buffer, sizeof(buffer), 0); } while (count < 0 && errno == EINTR);
        if (count < 0 && Blocked()) return;
        if (count <= 0) { NativeNotify(WOWC_DISCONNECTED); return; }
        {
            Response response(*this);
            if (response.value) response.value->WCDataReady(this, OsGetAsyncTimeMs(), buffer, int32_t(count));
        }
        if (m_native->generation != generation || m_connState != WOWC_CONNECTED) return;
    }
}
void WowConnection::DoMessageReads() {
    const auto generation = m_native->generation;
    {
        Response response(*this);
        if (response.value) response.value->NotifyAboutToDoReads();
    }
    if (m_native->generation != generation || m_connState != WOWC_CONNECTED) return;
    // Original 2/3-byte big-endian length and encrypted header/opcode span.
    // Read exactly the current frame, retaining partial headers across polls.
    for (unsigned budget = 0; budget < 128; ++budget) {
        int header = 2, total = -1;
        if (m_readBytes >= 2) {
            if (m_readBuffer[0] & 0x80) {
                header = 3;
                if (m_readBytes >= 3) total = 3 + ((m_readBuffer[0] & 0x7f) << 16 | m_readBuffer[1] << 8 | m_readBuffer[2]);
            } else total = 2 + (m_readBuffer[0] << 8 | m_readBuffer[1]);
        }
        const int target = total >= 0 ? total : header;
        if (target > m_readBufferSize) {
            m_readBuffer = static_cast<uint8_t*>(SMemReAlloc(m_readBuffer, target, __FILE__, __LINE__, 0));
            m_readBufferSize = target;
        }
        if (m_readBytes == total) {
            CDataStore msg(m_readBuffer + header, total - header);
            m_readBytes = 0;
            {
                Response response(*this);
                if (response.value) response.value->WCMessageReady(this, OsGetAsyncTimeMs(), &msg);
            }
            if (m_native->generation != generation || m_connState != WOWC_CONNECTED) return;
            continue;
        }
        ssize_t count;
        do { count = recv(m_sock, m_readBuffer + m_readBytes, target - m_readBytes, 0); } while (count < 0 && errno == EINTR);
        if (count < 0 && Blocked()) return;
        if (count <= 0) { NativeNotify(WOWC_DISCONNECTED); return; }
        if (m_encrypt) {
            const int length = std::clamp(header + int(m_uint376) - m_readBytes, 0, int(count));
            SARC4ProcessBuffer(m_readBuffer + m_readBytes, length, &m_receiveKey, &m_receiveKey);
        }
        m_readBytes += int(count);
    }
}
