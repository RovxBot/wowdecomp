// Native adaptation of the original WowConnectionNet owner. No donor code.
#include "Connection.h"
#include "WoW/Source/WowServices/WowConnectionNet.h"
#include <algorithm>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <netdb.h>
#include <poll.h>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <cerrno>

struct WowConnectionNet::Native {
    std::thread::id owner = std::this_thread::get_id();
    std::vector<WowConnection*> connections;
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<std::shared_ptr<WowNativeLookup>> lookups;
    bool stop = false;
    bool polling = false;
    bool closing = false;
    std::thread resolver;

    void Resolve() {
        for (;;) {
            std::shared_ptr<WowNativeLookup> lookup;
            {
                std::unique_lock<std::mutex> lock(mutex);
                wake.wait(lock, [&] { return stop || !lookups.empty(); });
                if (stop) return;
                lookup = lookups.front(); lookups.pop_front();
            }
            addrinfo hints{}, *result = nullptr;
            hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
            if (!getaddrinfo(lookup->host.c_str(), nullptr, &hints, &result)) {
                if (result && result->ai_addrlen >= sizeof(sockaddr_in)) {
                    lookup->address = reinterpret_cast<sockaddr_in*>(result->ai_addr)->sin_addr.s_addr;
                    lookup->success = true;
                }
                freeaddrinfo(result);
            }
            // Release/acquire publication; all callbacks and connection state
            // stay on Event's owner. Cancellation simply retires this result.
            lookup->done.store(true, std::memory_order_release);
        }
    }
};
WowConnectionNet::WowConnectionNet(int32_t threads, void (*)()) : m_native(std::make_unique<Native>()) {
    if (threads != 1) throw std::invalid_argument("Native connections require one Event owner");
    m_native->resolver = std::thread([this] { m_native->Resolve(); });
}
WowConnectionNet::~WowConnectionNet() { Stop(); }
void WowConnectionNet::CheckOwner() const {
    if (m_native->owner != std::this_thread::get_id())
        throw std::logic_error("WowConnectionNet requires its Event owner thread");
}
std::shared_ptr<WowNativeLookup> WowConnectionNet::Resolve(const char* host) {
    CheckOwner();
    auto lookup = std::make_shared<WowNativeLookup>(); lookup->host = host;
    std::lock_guard<std::mutex> lock(m_native->mutex);
    if (m_native->stop || m_native->lookups.size() >= 128)
        throw std::runtime_error("Native resolver is stopped or full");
    m_native->lookups.push_back(lookup); m_native->wake.notify_one();
    return lookup;
}
void WowConnectionNet::Add(WowConnection* connection) {
    CheckOwner();
    if (m_native->closing) throw std::logic_error("Native network is closing");
    if (std::find(m_native->connections.begin(), m_native->connections.end(), connection) == m_native->connections.end())
        m_native->connections.push_back(connection);
}
void WowConnectionNet::Remove(WowConnection* connection) {
    CheckOwner();
    auto& values = m_native->connections;
    values.erase(std::remove(values.begin(), values.end(), connection), values.end());
}
void WowConnectionNet::Delete(WowConnection* connection) { CheckOwner(); delete connection; }
void WowConnectionNet::PlatformDestruct(WowConnection*) { CheckOwner(); }
void WowConnectionNet::PlatformChangeState(WowConnection*, WOW_CONN_STATE) { CheckOwner(); }
void WowConnectionNet::Poll() {
    CheckOwner();
    if (m_native->polling || m_native->closing) throw std::logic_error("Native network poll reentry/shutdown");
    struct Snapshot {
        Native& owner;
        std::vector<WowConnection*> values;
        ~Snapshot() { for (auto* value : values) value->Release(); owner.polling = false; }
    } snapshot{*m_native, m_native->connections};
    m_native->polling = true;
    for (auto* value : snapshot.values) value->AddRef();
    std::vector<pollfd> descriptors;
    std::vector<uint64_t> generations;
    for (auto* value : snapshot.values) {
        short events = POLLIN;
        if (value->m_connState == WOWC_CONNECTING || value->m_sendList.Head() || value->m_notifyWriteReady) events |= POLLOUT;
        descriptors.push_back({value->m_sock, events, 0});
        generations.push_back(value->m_native->generation);
    }
    int result;
    do { result = poll(descriptors.data(), descriptors.size(), 0); } while (result < 0 && errno == EINTR);
    if (result < 0) throw std::runtime_error("Native socket poll failed");
    for (size_t i = 0; i < snapshot.values.size(); ++i) {
        auto* value = snapshot.values[i];
        // An earlier callback may release, cancel or reconnect another socket.
        // Never apply old readiness to a replacement descriptor (even fd reuse).
        if (value->m_native->generation == generations[i]) value->NativePoll(descriptors[i].revents);
    }
}
void WowConnectionNet::Stop() {
    CheckOwner();
    if (m_native->polling) throw std::logic_error("Finish network callbacks before shutdown");
    if (m_native->closing) return;
    m_native->closing = true;
    {
        std::lock_guard<std::mutex> lock(m_native->mutex);
        m_native->stop = true; m_native->lookups.clear(); m_native->wake.notify_one();
    }
    // POSIX getaddrinfo cannot cancel an in-progress libc lookup. Join it;
    // do not detach a worker or let it access destroyed storage.
    if (m_native->resolver.joinable()) m_native->resolver.join();
    for (auto* connection : m_native->connections) {
        connection->NativeReset(); connection->m_connState = WOWC_DISCONNECTED;
        connection->m_native->network = nullptr;
    }
    m_native->connections.clear();
}
