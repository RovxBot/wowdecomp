// Native ownership adaptation of the original Grunt timer and NETEVENTQUEUE.
// Windows implementations remain in their original translation units.
#include "Session.h"
#include "Engine/Source/Os/TimeManager.h"
#include "WoW/Source/Net/NetClient/NetClient.h"
#include "WoW/Source/WowServices/Grunt.h"
#include "WoW/Source/WowServices/WowConnectionNet.h"
#include <algorithm>
#include <list>
#include <stdexcept>
#include <utility>
#include <vector>
namespace {
uint64_t serial = 0;
using TimerEntry = std::pair<Grunt::Timer *, uint64_t>;
using ClientEntry = std::pair<NetClient *, uint64_t>;
std::list<TimerEntry> timers;
std::list<ClientEntry> clients;
bool polling = false;
} // namespace
namespace NativeSession {
void CheckOwner() {
    if (!WowConnection::s_network)
        throw std::logic_error("Native sessions require the Event transport owner");
    WowConnection::s_network->CheckOwner();
}
void Attach(NetClient *client) {
    CheckOwner();
    clients.emplace_back(client, ++serial);
}
void Detach(NetClient *client) {
    CheckOwner();
    clients.remove_if([&](const ClientEntry &e) { return e.first == client; });
}
void Poll() {
    CheckOwner();
    if (polling)
        throw std::logic_error("Native session poll cannot reenter");
    polling = true;
    struct Scope {
        ~Scope() { polling = false; }
    } scope;
    // Stable identities avoid dereferencing an owner deleted/recreated by an earlier callback.
    const std::vector<TimerEntry> timerSnapshot(timers.begin(), timers.end());
    for (const auto &e : timerSnapshot)
        if (std::find(timers.begin(), timers.end(), e) != timers.end())
            e.first->Pump();
    const std::vector<ClientEntry> clientSnapshot(clients.begin(), clients.end());
    for (const auto &e : clientSnapshot)
        if (std::find(clients.begin(), clients.end(), e) != clients.end()) {
            e.first->PollNativeAuthWork();
            e.first->PollEventQueue();
        }
}
} // namespace NativeSession
Grunt::Timer::Timer() : m_event(0, 0) {
    NativeSession::CheckOwner();
    timers.emplace_back(this, ++serial);
}
Grunt::Timer::~Timer() { this->Shutdown(); }
void Grunt::Timer::Shutdown() {
    NativeSession::CheckOwner();
    timers.remove_if([&](const TimerEntry &e) { return e.first == this; });
    this->m_list.UnlinkAll();
}
void Grunt::Timer::Insert(Grunt::Timer::Event &event) {
    NativeSession::CheckOwner();
    if (!event.m_interval)
        throw std::invalid_argument("Native Grunt timer needs an interval");
    event.m_nextTime = OsGetAsyncTimeMs() + event.m_interval;
    this->m_list.LinkToTail(&event);
}
int32_t Grunt::Timer::Pump() {
    NativeSession::CheckOwner();
    // ClientLink owns one repeating event. Advance before Call; Call may delete it.
    auto event = this->m_list.Head();
    if (!event)
        return 100;
    const int32_t remaining = int32_t(event->m_nextTime - OsGetAsyncTimeMs());
    if (remaining > 0)
        return remaining;
    event->m_nextTime = OsGetAsyncTimeMs() + event->m_interval;
    event->Call();
    return 100;
}

int32_t NetClient::Initialize() {
    NativeSession::CheckOwner();
    if (this->m_netState != NS_UNINITIALIZED)
        throw std::logic_error("NetClient is already initialized");
    this->m_netEventQueue = new NETEVENTQUEUE(this);
    memset(this->m_handlers, 0, sizeof(this->m_handlers));
    memset(this->m_handlerParams, 0, sizeof(this->m_handlerParams));
    memset(&this->m_loginData, 0, sizeof(this->m_loginData));
    this->m_serverConnection = new WowConnection(this, nullptr);
    this->m_netState = NS_INITIALIZED;
    ++s_clientCount;
    NativeSession::Attach(this);
    return 1;
}
void NetClient::Destroy() {
    if (this->m_netState == NS_UNINITIALIZED)
        return;
    NativeSession::CheckOwner();
    this->CancelNativeAuthWork();
    if (this->m_netEventQueue->m_polling)
        throw std::logic_error("Use DelayedDelete during network event dispatch");
    NativeSession::Detach(this);
    for (auto connection : {this->m_serverConnection, this->m_redirectConnection}) {
        if (!connection)
            continue;
        connection->SetResponse(nullptr, false);
        connection->Disconnect();
        connection->Release();
    }
    this->m_serverConnection = this->m_redirectConnection = nullptr;
    delete this->m_netEventQueue;
    this->m_netEventQueue = nullptr;
    this->m_heldMessages.Clear();
    volatile uint8_t* loginBytes = reinterpret_cast<volatile uint8_t*>(&this->m_loginData);
    for (size_t i=0; i<sizeof(this->m_loginData); ++i) loginBytes[i]=0;
    this->m_netState = NS_UNINITIALIZED;
    --s_clientCount;
    // Event owns transport lifetime; destroying the last session must not retire it.
}
void NETEVENTQUEUE::Clear() {
    NativeSession::CheckOwner();
    if (this->m_polling)
        throw std::logic_error("Cannot clear a dispatching network queue");
    while (auto node = this->m_eventQueue.Head()) {
        SMemFree(node->m_data, __FILE__, __LINE__, 0);
        this->m_eventQueue.DeleteNode(node);
        this->m_client->m_refCount.Decrement();
    }
}

void NetClient::CancelNativeAuthWork() {
    NativeSession::CheckOwner();
    m_nativeAuthWork = NativeAuthWork{};
}
void NetClient::BeginNativeAuthChallenge(WowConnection* connection, CDataStore* msg) {
    NativeSession::CheckOwner();
    auto& work = m_nativeAuthWork;
    if (connection != m_serverConnection || work.challenged || connection->m_encrypt ||
        msg->m_read > msg->m_size || msg->m_size - msg->m_read != 40 || !m_loginData.m_account[0]) {
        work.pending = false;
        connection->Disconnect();
        return;
    }
    work.challenged = true;
    msg->Get(work.difficulty).Get(work.challenge.m_serverSeed);
    if (work.difficulty > 160) { connection->Disconnect(); return; }
    SHA1_Init(&work.context);
    SHA1_Update(&work.context, reinterpret_cast<const uint8_t*>(m_loginData.m_account),
                SStrLen(m_loginData.m_account));
    // Hash original wire bytes: eight LE uint32 seeds, independent of host ABI.
    SHA1_Update(&work.context, msg->m_data + msg->m_read, 32);
    msg->m_read += 32;
    work.started = OsGetAsyncTimeMs();
    work.pending = true;
}
void NetClient::PollNativeAuthWork() {
    NativeSession::CheckOwner();
    auto& work = m_nativeAuthWork;
    if (!work.pending) return;
    if (m_deleteMe || m_netState != NS_CONNECTED) {
        // A connect event can still be queued from this same socket poll.
        if (m_netState == NS_CONNECTING && !m_deleteMe) return;
        work.pending = false;
        return;
    }
    const uint32_t start = OsGetAsyncTimeMs();
    if (uint32_t(start - work.started) >= 30000) {
        work.pending = false;
        m_serverConnection->Disconnect();
        return;
    }
    // Same nonce search and low-bit count as 0x006321B0, with bounded Event
    // slices (1024 attempts / 2 ms) and an overall cancellable 30-second budget.
    for (unsigned attempt = 0; attempt < 1024; ++attempt) {
        SHA1_CONTEXT context = work.context;
        uint8_t nonce[8];
        for (unsigned i = 0; i < 8; ++i) nonce[i] = uint8_t(work.nonce >> (i * 8));
        SHA1_Update(&context, nonce, sizeof(nonce));
        uint8_t digest[20];
        SHA1_Final(digest, &context);
        unsigned bits = 0;
        for (uint8_t byte : digest) {
            if (!byte) { bits += 8; continue; }
            while (!(byte & 1)) { ++bits; byte >>= 1; }
            break;
        }
        if (bits >= work.difficulty) {
            work.challenge.m_proof = work.nonce;
            work.pending = false;
            m_netEventQueue->AddEvent(EVENT_ID_NET_AUTH_CHALLENGE, m_serverConnection, this,
                                     &work.challenge, sizeof(work.challenge));
            return;
        }
        if (++work.nonce == 0) {
            work.pending = false;
            m_serverConnection->Disconnect();
            return;
        }
        if (uint32_t(OsGetAsyncTimeMs() - start) >= 2) return;
    }
}
NETEVENTQUEUE::~NETEVENTQUEUE() { this->Clear(); }
void NETEVENTQUEUE::Poll() {
    NativeSession::CheckOwner();
    if (this->m_polling)
        throw std::logic_error("Network event queue cannot reenter");
    this->m_polling = true;
    NetClient *client = this->m_client;
    client->m_refCount.Increment();
    struct Scope {
        NETEVENTQUEUE *queue;
        NetClient *client;
        ~Scope() {
            queue->m_polling = false;
            if (!client->m_refCount.Decrement() && client->m_deleteMe)
                delete client;
        }
    } scope{this, client};
    // Only events pending at entry; callbacks' new events wait for the next poll.
    size_t count = 0;
    for (auto node = this->m_eventQueue.Head(); node; node = this->m_eventQueue.Next(node))
        ++count;
    while (count--) {
        auto node = this->m_eventQueue.Head();
        const auto event = node->m_eventId;
        const auto time = node->m_timeReceived;
        const auto size = node->m_dataSize;
        void *data = node->m_data;
        this->m_eventQueue.DeleteNode(node);
        struct Payload {
            void *data;
            NetClient *client;
            ~Payload() {
                SMemFree(data, __FILE__, __LINE__, 0);
                client->m_refCount.Decrement();
            }
        } payload{data, client};
        if (client->m_deleteMe)
            continue;
        switch (event) {
        case EVENT_ID_NET_DATA:
            client->HandleData(time, data, size);
            break;
        case EVENT_ID_NET_CONNECT:
            client->HandleConnect();
            break;
        case EVENT_ID_NET_DISCONNECT:
            client->HandleDisconnect();
            break;
        case EVENT_ID_NET_CANTCONNECT:
            client->HandleCantConnect();
            break;
        case EVENT_ID_NET_AUTH_CHALLENGE:
            client->HandleAuthChallenge(static_cast<AuthenticationChallenge *>(data));
            break;
        case EVENT_ID_NET_DESTROY:
            client->m_deleteMe = true;
            break;
        default:
            throw std::logic_error("Unsupported native network event");
        }
    }
    if (!client->m_deleteMe)
        client->HandleIdle();
}

int32_t NetClient::DelayedDelete() {
    NativeSession::CheckOwner();
    this->CancelNativeAuthWork();
    if (this->m_netState == NS_UNINITIALIZED)
        return 0;
    if (this->m_serverConnection)
        this->m_serverConnection->Disconnect();
    if (this->m_redirectConnection)
        this->m_redirectConnection->Disconnect();
    this->m_netEventQueue->AddEvent(EVENT_ID_NET_DESTROY, nullptr, this, nullptr, 0);
    return 1;
}

int32_t Grunt::ClientLink::CmdRealmList(CDataStore &msg) {
    if (this->m_state != 6)
        return 1;
    if (!CanReadNumBytes(msg, 2))
        return 0;
    uint16_t size = 0;
    msg.Get(size);
    if (!CanReadNumBytes(msg, size))
        return 0;
    if (size < 8)
        return 1;
    CDataStore packet(msg.m_data + msg.m_read, size);
    uint32_t unknown = 0;
    uint16_t count = 0;
    packet.Get(unknown).Get(count);
    if (count > (size - 8u) / 12u)
        return 1;
    const uint32_t realmStart = 4;
    for (uint32_t i = 0; i < count; ++i) {
        uint8_t type = 0, locked = 0, flags = 0, characters = 0, zone = 0, id = 0;
        char name[256], address[256];
        float population = 0;
        packet.Get(type).Get(locked).Get(flags);
        packet.GetString(name, sizeof(name)).GetString(address, sizeof(address));
        packet.Get(population).Get(characters).Get(zone).Get(id);
        if (flags & 4) {
            uint8_t major = 0, minor = 0, patch = 0;
            uint16_t build = 0;
            packet.Get(major).Get(minor).Get(patch).Get(build);
        }
        if (packet.m_read > packet.m_size)
            return 1;
    }
    uint16_t trailer = 0;
    packet.Get(trailer);
    if (packet.m_read != packet.m_size)
        return 1;
    packet.m_read = realmStart;
    const auto alive = this->m_nativeAlive;
    this->m_clientResponse->RealmListResult(&packet);
    if (*alive)
        msg.m_read += size;
    return 2;
}
