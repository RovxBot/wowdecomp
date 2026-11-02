// Controlled loopback protocol peer. Identities, credentials, executable bytes
// and realms below are authored fixtures, never native platform/integrity claims.
#include "WoW/Source/Net/NetClient/NetClient.h"
#include "WoW/Source/WowServices/BigInteger.h"
#include "WoW/Source/WowServices/GruntLogin.h"
#include "WoW/Source/WowServices/LoginResponse.h"
#include "WoW/Source/WowServices/Portable/Session.h"
#include "WoW/Source/WowServices/SRP6_Random.h"
#include "WoW/Source/WowServices/WowConnectionNet.h"
#include "Engine/Source/Os/OsVersionHash.h"
#include "WoW/Source/WowServices/HMAC.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/ChecksumExecutables.h"
#if defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN)
#include "WoW/Source/WowServices/Portable/TestServerLogin.h"
#include "test_server_peer.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"
#endif
#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>
namespace {
using Bytes = std::vector<uint8_t>;
void Require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
Bytes Hex(const char *text) {
    Bytes result;
    while (*text) {
        auto digit = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
        result.push_back(uint8_t(digit(text[0]) * 16 + digit(text[1])));
        text += 2;
    }
    return result;
}
#include "auth_vectors.h"
Bytes Hash(const Bytes &bytes) {
    Bytes result(20);
    SHA1_CONTEXT context;
    SHA1_Init(&context);
    SHA1_Update(&context, bytes.data(), bytes.size());
    SHA1_Final(result.data(), &context);
    return result;
}
void Append(Bytes &out, const Bytes &in) { out.insert(out.end(), in.begin(), in.end()); }
Bytes Wire(CDataStore &msg) { return Bytes(msg.m_data, msg.m_data + msg.m_size); }
struct Big {
    BigInteger n;
    explicit Big(const Bytes &b) : n(BigIntegerFromBytes(b.data(), b.size())) {}
    Big() : n(BigIntegerFromInt(0)) {}
    ~Big() { BigIntegerFree(n); }
    Big(const Big &) = delete;
    Bytes Bytes32() {
        Bytes b(32);
        BigIntegerToBytes(n, b.data(), b.size());
        return b;
    }
};
// Server-side algebra uses received A and independently checks M1, version proof
// and M2. Fixed client vectors above also check the arithmetic/hash primitives.
Bytes ServerSession(const Bytes &a) {
    Big N(Prime), A(a), v(Verifier), b(ServerPrivate), u(Hash([&] {
        Bytes x = a;
        Append(x, ServerPublic);
        return x;
    }())),
        t, s;
    BigIntegerModExp(t.n, v.n, u.n, N.n);
    BigIntegerMul(t.n, A.n, t.n);
    BigIntegerModExp(s.n, t.n, b.n, N.n);
    Bytes key(40), secret = s.Bytes32();
    SHA1_InterleaveHash(key.data(), secret.data(), secret.size());
    return key;
}
Bytes ClientProof(const Bytes &a, const Bytes &key) {
    Bytes x = Hash(Prime), y = Hash(Bytes{7});
    for (size_t i = 0; i < x.size(); ++i)
        x[i] ^= y[i];
    Append(x, Hash(Bytes{'F', 'I', 'X', 'T', 'U', 'R', 'E'}));
    Append(x, Salt);
    Append(x, a);
    Append(x, ServerPublic);
    Append(x, key);
    return Hash(x);
}
struct Fd {
    int n = -1;
    explicit Fd(int v = -1) : n(v) {}
    ~Fd() { Close(); }
    void Close() {
        if (n >= 0)
            close(n);
        n = -1;
    }
};
struct ExecutableFixture {
    std::string path;
    Fd file;
    ExecutableFixture() {
        auto name = (std::filesystem::temp_directory_path() / "wowdecomp-version-XXXXXX").string();
        std::vector<char> buffer(name.begin(), name.end());
        buffer.push_back(0);
        file.n = mkstemp(buffer.data());
        Require(file.n >= 0, "Create authored executable fixture");
        path = buffer.data();
    }
    ~ExecutableFixture() { unlink(path.c_str()); }
    void Write(const char* bytes) {
        const size_t length = strlen(bytes);
        Require(write(file.n, bytes, length) == ssize_t(length), "Write authored executable bytes");
    }
};
Bytes FileChecksum(const uint8_t* challenge) {
    ExecutableFixture file;
    file.Write("authored-exe");
    HMAC<SHA1> hmac;
    hmac.Init(challenge, 16);
    IOsVersionHashCallback2<HMAC<SHA1>> callback(&hmac);
    Require(IOsVersionChecksum(callback, file.path.c_str(), 0), "Native raw-file checksum failed");
    SHA1::Digest digest;
    hmac.Finish(digest);
    return Bytes(digest.m_data, digest.m_data + sizeof(digest.m_data));
}
void VersionChecksums() {
    const auto challenge = Hex("a0a1a2a3a4a5a6a7a8a9aaabacadaeaf");
    Require(FileChecksum(challenge.data()) == ExpectedVersionChecksum,
            "Native file HMAC differs from independent Python vector");
    struct Callback : IOsVersionHashCallback {
        unsigned calls = 0;
        Bytes bytes;
        void Process(const void* data, uint32_t size) override {
            ++calls;
            const auto* begin = static_cast<const uint8_t*>(data);
            bytes.assign(begin, begin + size);
        }
    } callback;
    ExecutableFixture file;
    Require(!IOsVersionChecksum(callback, nullptr, 0) &&
                !IOsVersionChecksum(callback, "", 0) &&
                !IOsVersionChecksum(callback, nullptr, 1) &&
                !IOsVersionChecksum(callback, file.path.c_str(), 1) &&
                !IOsVersionChecksum(callback, (file.path + "/missing").c_str(), 0) &&
                !IOsVersionChecksum(callback, std::filesystem::temp_directory_path().c_str(), 0) &&
                !callback.calls,
            "Unsupported checksum input reached callback");
    Require(IOsVersionChecksum(callback, file.path.c_str(), 0) && callback.calls == 1 &&
                callback.bytes.empty(), "Empty raw file was not delivered exactly once");
    // Reject a sparse extent wider than the original uint32 callback before allocation.
    Require(!ftruncate(file.file.n, int64_t(UINT32_MAX) + 1), "Create wide checksum fixture");
    Require(!IOsVersionChecksum(callback, file.path.c_str(), 0) && callback.calls == 1,
            "Wide file truncated into checksum callback");
    Require(!ftruncate(file.file.n, 0), "Reset checksum fixture");
    std::string content(65539, 'x');
    file.Write(content.c_str());
    Require(IOsVersionChecksum(callback, file.path.c_str(), 0) && callback.calls == 2 &&
                callback.bytes == Bytes(content.begin(), content.end()),
            "Raw checksum did not deliver complete ordered bytes");
    const auto fifo = file.path + ".fifo";
    Require(!mkfifo(fifo.c_str(), 0600), "Create nonregular checksum fixture");
    const bool accepted = IOsVersionChecksum(callback, fifo.c_str(), 0);
    unlink(fifo.c_str());
    Require(!accepted && callback.calls == 2, "Nonregular file reached checksum callback");
    Bytes output(20, 0x5a), sentinel = output;
    bool rejected = false;
    try { ChecksumExecutables(nullptr, UINT32_MAX, output.data()); }
    catch (const std::runtime_error&) { rejected = true; }
    Require(rejected && output == sentinel,
            "Unsupported executable inventory published a version checksum");
}
struct Peer {
    Fd listener{socket(AF_INET, SOCK_STREAM, 0)}, stream;
    uint16_t port = 0;
    Peer() {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        Require(listener.n >= 0 &&
                    !bind(listener.n, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) &&
                    !listen(listener.n, 2),
                "Auth fixture listen failed");
        socklen_t size = sizeof(addr);
        Require(!getsockname(listener.n, reinterpret_cast<sockaddr *>(&addr), &size),
                "Auth fixture port missing");
        port = ntohs(addr.sin_port);
        fcntl(listener.n, F_SETFL, O_NONBLOCK);
    }
    std::string Address() { return "127.0.0.1:" + std::to_string(port); }
    void Accept() {
        if (stream.n < 0) {
            stream.n = accept(listener.n, nullptr, nullptr);
            if (stream.n >= 0)
                fcntl(stream.n, F_SETFL, O_NONBLOCK);
        }
    }
    void Put(const Bytes &b) {
        Require(send(stream.n, b.data(), b.size(), 0) == ssize_t(b.size()),
                "Auth fixture write failed");
    }
    Bytes Read() {
        Bytes out;
        uint8_t b[1024];
        ssize_t n;
        while ((n = recv(stream.n, b, sizeof(b), 0)) > 0)
            out.insert(out.end(), b, b + n);
        return out;
    }
};
void Until(void (*pump)(), std::function<bool()> done, const char *message) {
    auto end = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    while (!done()) {
        Require(std::chrono::steady_clock::now() < end, message);
        pump();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
Bytes Receive(Peer &p, void (*pump)(), size_t size) {
    Bytes out;
    Until(
        pump,
        [&] {
            Append(out, p.Read());
            return out.size() >= size;
        },
        "Protocol peer receive timed out");
    Require(out.size() == size, "Unexpected extra protocol bytes");
    return out;
}
void Fragment(Peer &p, void (*pump)(), const Bytes &b) {
    for (uint8_t c : b) {
        p.Put(Bytes{c});
        pump();
    }
}
struct Response : LoginResponse {
    std::vector<LOGIN_STATE> states;
    std::function<void(LOGIN_STATE)> onStatus;
    unsigned realms = 0;
    uint32_t realmResult = 999;
    bool sawIdentityFailure = false;
    const char *GetLoginServer() override { return nullptr; }
    int32_t GetLoginServerType() override { return 0; }
    void JoinRealmResult(uint32_t, int32_t, int32_t, int32_t, int32_t) override {
        throw std::runtime_error("Unexpected realm join");
    }
    void LoginServerStatus(LOGIN_STATE s, LOGIN_RESULT result, const char *, const char *,
                           const char *, uint16_t) override {
        states.push_back(s);
        if (s == LOGIN_STATE_FAILED && result == LOGIN_FAILED)
            sawIdentityFailure = true;
        if (onStatus)
            onStatus(s);
    }
    void RealmEnumCallback(uint32_t r) override {
        ++realms;
        realmResult = r;
    }
};
struct FixtureLogin : GruntLogin {
    // This test-only callback supplies an authored protocol identity. Production
    // GruntLogin refuses identity selection and UI DefaultServerLogin stays gated.
    void GetLogonMethod() override {
        // Original GruntLogin retries only while a credential buffer remains.
        if (!this->m_password)
            return;
        Grunt::ClientLink::Logon identity{};
        identity.m_programId = 0x576f57;
        identity.m_processor = 0x783836;
        identity.m_os = 0x57696e;
        identity.m_locale = 0x656e5553;
        identity.m_versionMajor = 3;
        identity.m_versionMinor = 3;
        identity.m_versionPatch = 5;
        identity.m_build = 12340;
        identity.m_accountName = this->m_username;
        identity.m_password = this->m_password;
        this->m_clientLink->LogonNewSession(identity);
        memset(this->m_password, 0, strlen(this->m_password));
        SMemFree(this->m_password, __FILE__, __LINE__, 0);
        this->m_password = nullptr;
    }
};
Bytes Challenge() {
    CDataStore p;
    p.Put(uint8_t(0)).Put(uint8_t(0)).Put(uint8_t(0));
    p.PutData(ServerPublic.data(), 32);
    p.Put(uint8_t(1)).Put(uint8_t(7)).Put(uint8_t(32));
    p.PutData(Prime.data(), 32);
    p.PutData(Salt.data(), 32);
    for (unsigned i = 0; i < 16; ++i)
        p.Put(uint8_t(0xa0 + i));
    p.Put(uint8_t(0));
    return Wire(p);
}
Bytes RealmPacket(bool malformed = false) {
    CDataStore p;
    p.Put(uint8_t(0x10)).Put(uint16_t(0)).Put(uint32_t(0)).Put(uint16_t(2));
    for (unsigned i = 0; i < 2; ++i) {
        p.Put(uint8_t(1)).Put(uint8_t(i)).Put(uint8_t(i ? 4 : 0));
        p.PutString(i ? "Fixture Two" : "Fixture/One. ");
        p.PutString(i ? "127.0.0.1:23456" : "127.0.0.1:12345");
        p.Put(1.5f).Put(uint8_t(3)).Put(uint8_t(1)).Put(uint8_t(7 + i));
        if (i)
            p.Put(uint8_t(3)).Put(uint8_t(3)).Put(uint8_t(5)).Put(uint16_t(12340));
    }
    p.Put(uint16_t(0x10));
    p.Set(1, uint16_t(p.m_size - 3 - (malformed ? 1 : 0)));
    return Wire(p);
}
void Vectors() {
    SRP6_Client client{};
    client.BeginAuthentication("FIXTURE", "AUTHORED-PASSWORD");
    uint8_t seed[16];
    for (unsigned i = 0; i < 16; ++i)
        seed[i] = i;
    SRP6_Random random(seed, 16);
    uint8_t g = 7;
    Require(client.CalculateProof(Prime.data(), 32, &g, 1, Salt.data(), 32, ServerPublic.data(), 32,
                                  random) == 0,
            "Known SRP vector calculation failed");
    Require(!memcmp(client.m_publicKey, ExpectedA.data(), 32) &&
                !memcmp(client.m_clientProof, ExpectedM.data(), 20) &&
                !memcmp(client.m_sessionKey, ExpectedK.data(), 40),
            "Independent Python SRP vector mismatch");
    Require(client.VerifyServerProof(ExpectedM2.data(), 20) == 0, "Known server proof rejected");
    Require(client.CalculateProof(nullptr, 32, &g, 1, Salt.data(), 32, ServerPublic.data(), 32,
                                  random) != 0,
            "Null SRP modulus accepted");
    Bytes zero(32);
    Require(client.CalculateProof(Prime.data(), 32, &g, 1, Salt.data(), 32, zero.data(), 32,
                                  random) != 0,
            "Zero server public key accepted");
}
void Authenticate(void (*pump)(), bool badProof, bool malformedRealm,
                  LoginData *handoff = nullptr) {
    Peer peer;
    Response response;
    FixtureLogin login;
    login.Init(&response);
    login.SetLogonCreds("fixture", "authored-password");
    login.Logon(peer.Address().c_str(), nullptr);
    Until(
        pump,
        [&] {
            peer.Accept();
            return peer.stream.n >= 0 && login.m_clientLink->m_state == 3;
        },
        "Original Grunt logon did not start");
    Bytes hello = Receive(peer, pump, 41);
    Require(hello[0] == 0 && hello[1] == 8 && hello[2] == 37 && hello[3] == 0 && hello[33] == 7 &&
                std::string(hello.begin() + 34, hello.end()) == "FIXTURE",
            "Original logon serialization changed");
    auto challenge = Challenge();
    Fragment(peer, pump, Bytes(challenge.begin(), challenge.end() - 1));
    Require(login.m_clientLink->m_state == 3, "Partial challenge advanced authentication");
    peer.Put(Bytes{challenge.back()});
    Until(
        pump, [&] { return login.m_clientLink->m_state == 4; },
        "Original challenge/security callbacks failed");
    Require(response.states.back() == LOGIN_STATE_CHECKINGVERSIONS && peer.Read().empty(),
            "Version prerequisite was bypassed");
    LoginData data{};
    bool pending = false;
    try {
        login.Wait(nullptr, data);
    } catch (const std::logic_error &) {
        pending = true;
    }
    Require(pending, "Blocking native Wait accepted");
    Bytes checksum = FileChecksum(login.GetVersionChallenge());
    login.ProveVersion(checksum.data());
    Bytes proof = Receive(peer, pump, 75), a(proof.begin() + 1, proof.begin() + 33),
          key = ServerSession(a), expected = ClientProof(a, key);
    Require(proof[0] == 1 && proof[73] == 0 && proof[74] == 0 &&
                !memcmp(proof.data() + 33, expected.data(), 20),
            "Peer rejected SRP client proof");
    Bytes version = a;
    Append(version, ExpectedVersionChecksum);
    Require(!memcmp(proof.data() + 53, Hash(version).data(), 20), "Version proof bytes changed");
    Bytes m2 = a;
    Append(m2, expected);
    Append(m2, key);
    m2 = Hash(m2);
    if (badProof)
        m2[0] ^= 1;
    CDataStore reply;
    reply.Put(uint8_t(1)).Put(uint8_t(0));
    reply.PutData(m2.data(), 20);
    reply.Put(uint32_t(8)).Put(uint32_t(0)).Put(uint16_t(0));
    Bytes wire = Wire(reply);
    Fragment(peer, pump, Bytes(wire.begin(), wire.end() - 2));
    Require(login.m_logonStatus == -1, "Truncated proof published a result");
    peer.Put(Bytes(wire.end() - 2, wire.end()));
    Until(pump, [&] { return login.m_logonStatus != -1; }, "Original proof callback missing");
    if (badProof) {
        Require(login.m_logonStatus == 1 && response.states.back() == LOGIN_STATE_FAILED,
                "Bad server proof authenticated");
        return;
    }
    Require(login.Wait(nullptr, data) && !memcmp(data.m_sessionKey, key.data(), 40) &&
                response.states.back() == LOGIN_STATE_AUTHENTICATED,
            "Verified proof/session handoff failed");
    if (handoff)
        *handoff = data;
    login.GetRealmList();
    Require(Receive(peer, pump, 5) == Bytes({0x10, 0, 0, 0, 0}), "Realm request bytes changed");
    auto realms = RealmPacket(malformedRealm);
    if (!malformedRealm)
        Append(realms, RealmPacket());
    peer.Put(realms);
    if (malformedRealm) {
        Until(
            pump, [&] { return !login.m_clientLink->m_connection; },
            "Malformed realm length did not disconnect");
        Require(!response.realms, "Malformed realm reached consumer");
        return;
    }
    Until(pump, [&] { return response.realms == 2; }, "Coalesced realm messages failed");
    Require(response.realmResult == 0 && response.m_realmList.Count() == 2 &&
                response.m_realmListTrailer == 0x10,
            "Original realm consumer rejected bounded messages");
    auto &r = response.m_realmList[0];
    Require(!strcmp(r.m_realmName, "Fixture One") && !strcmp(r.m_realmAddress, "127.0.0.1:12345") &&
                r.m_realmId == 7,
            "Advertised fixture realm fields changed");
    Require(response.m_realmList[1].m_versionBuild == 12340, "Versioned realm extension lost");
}
struct Client : NetClient {
    std::vector<int> events;
    bool deleteOnConnect = false;
    bool reentryRejected = false;
    unsigned *deleted = nullptr;
    ~Client() override {
        if (deleted)
            ++*deleted;
    }
    int32_t HandleConnect() override {
        events.push_back(1);
        if (deleteOnConnect) {
            this->DelayedDelete();
            return 1;
        }
        return NetClient::HandleConnect();
    }
    int32_t HandleDisconnect() override {
        events.push_back(3);
        return NetClient::HandleDisconnect();
    }
    int32_t HandleCantConnect() override {
        events.push_back(4);
        return NetClient::HandleCantConnect();
    }
    void HandleAuthChallenge(AuthenticationChallenge *c) override {
        Require(c->m_serverSeed == 0x12345678 && c->m_proof == 0,
                "World challenge routing changed");
        events.push_back(2);
        try {
            this->PollEventQueue();
        } catch (const std::logic_error &) {
            reentryRejected = true;
        }
    }
};
void WorldQueue(void (*pump)(), LoginData &handoff) {
    Peer peer;
    Client client;
    client.Initialize();
    client.SetLoginData(&handoff);
    Require(!memcmp(client.GetSessionKey(), handoff.m_sessionKey, 40),
            "Grunt-to-NetClient session key changed");
    client.Connect(peer.Address().c_str());
    Until(
        pump,
        [&] {
            peer.Accept();
            return client.m_netState == NS_CONNECTED;
        },
        "Original NetClient connect queue failed");
    CDataStore challenge;
    challenge.Put(uint16_t(SMSG_AUTH_CHALLENGE)).Put(uint32_t(0)).Put(uint32_t(0x12345678));
    for (unsigned i = 0; i < 8; ++i)
        challenge.Put(uint32_t(i));
    Bytes wire{0, uint8_t(challenge.m_size)};
    Append(wire, Wire(challenge));
    Fragment(peer, pump, wire);
    Until(pump, [&] { return client.events.size() == 2; }, "Original NetClient auth event missing");
    Require(client.events == std::vector<int>({1, 2}) && client.m_refCount.m_value == 0,
            "Network event ordering/ref ownership failed");
    Require(client.reentryRejected, "Queue callback reentry accepted");
    bool offOwnerRejected = false;
    std::thread foreign([&] {
        try {
            NativeSession::Poll();
        } catch (const std::logic_error &) {
            offOwnerRejected = true;
        }
    });
    foreign.join();
    Require(offOwnerRejected, "Foreign-thread session polling accepted");
    unsigned messages = 0;
    client.SetMessageHandler(
        SMSG_AUTH_RESPONSE,
        +[](void *p, NETMESSAGE, uint32_t, CDataStore *m) -> int32_t {
            uint8_t result = 0;
            m->Get(result);
            Require(result == 13, "Fixture failure response changed");
            ++*static_cast<unsigned *>(p);
            return 1;
        },
        &messages);
    peer.Put(Bytes{0, 3, uint8_t(SMSG_AUTH_RESPONSE), uint8_t(SMSG_AUTH_RESPONSE >> 8), 13});
    Until(pump, [&] { return messages == 1; }, "Original message handler not dispatched");
    peer.stream.Close();
    Until(
        pump, [&] { return client.m_netState == NS_INITIALIZED; },
        "Original NetClient disconnect missing");
    Require(client.events.back() == 3, "Disconnect event lost");
    client.Destroy();
    Require(WowConnection::s_network != nullptr, "Session destroyed Event transport");
    // Queued payload disposal, cancellation and deletion requested by a callback.
    unsigned deleted = 0;
    auto deferred = new Client;
    deferred->deleted = &deleted;
    deferred->Initialize();
    deferred->deleteOnConnect = true;
    deferred->m_netEventQueue->AddEvent(EVENT_ID_NET_CONNECT, nullptr, deferred, nullptr, 0);
    pump();
    pump();
    Require(deleted == 1, "DelayedDelete did not release queue/client");
    Client cancelled;
    cancelled.Initialize();
    uint8_t byte = 0;
    cancelled.m_netEventQueue->AddEvent(EVENT_ID_NET_DATA, nullptr, &cancelled, &byte, 1);
    cancelled.Destroy();
    Require(cancelled.m_refCount.m_value == 0, "Cancelled event retained a reference");
}
void FailurePaths(void (*pump)()) {
    // The production callback cannot supply an approved identity or emit credentials.
    {
        Peer peer;
        Response response;
        GruntLogin login;
        login.Init(&response);
        login.SetLogonCreds("fixture", "authored-password");
        login.Logon(peer.Address().c_str(), nullptr);
        Until(
            pump,
            [&] {
                peer.Accept();
                return login.m_logonStatus == 1 && !login.m_clientLink->m_connection;
            },
            "Unsupported production identity did not fail");
        Require(peer.Read().empty(), "Production native identity forwarded a logon");
        Require(response.sawIdentityFailure,
                "Native identity refusal was not reported before disconnect");
        Require(!login.m_password, "Native identity refusal retained pending credentials");
    }
    for (auto releasedState : {LOGIN_STATE_CONNECTING, LOGIN_STATE_FAILED}) {
        Peer peer;
        Response response;
        auto login = std::make_unique<GruntLogin>();
        login->Init(&response);
        login->SetLogonCreds("fixture", "authored-password");
        response.onStatus = [&](LOGIN_STATE state) {
            if (state == releasedState)
                login.reset();
        };
        login->Logon(peer.Address().c_str(), nullptr);
        Until(
            pump,
            [&] {
                peer.Accept();
                return !login;
            },
            "Login status callback release failed");
        pump();
    }
    // Security callback releases Login/ClientLink while command dispatch is active.
    {
        Peer peer;
        Response response;
        auto login = std::make_unique<FixtureLogin>();
        login->Init(&response);
        login->SetLogonCreds("fixture", "authored-password");
        login->Logon(peer.Address().c_str(), nullptr);
        Until(
            pump,
            [&] {
                peer.Accept();
                return login->m_clientLink->m_state == 3 && peer.stream.n >= 0;
            },
            "Callback lifetime fixture did not connect");
        Receive(peer, pump, 41);
        response.onStatus = [&](LOGIN_STATE state) {
            if (state == LOGIN_STATE_CHECKINGVERSIONS)
                login.reset();
        };
        peer.Put(Challenge());
        Until(pump, [&] { return !login; }, "Security callback release was not delivered");
        pump();
    }
    // A proof cannot establish a session before challenge/version processing.
    {
        Peer peer;
        Response response;
        FixtureLogin login;
        login.Init(&response);
        login.SetLogonCreds("fixture", "authored-password");
        login.Logon(peer.Address().c_str(), nullptr);
        Until(
            pump,
            [&] {
                peer.Accept();
                return login.m_clientLink->m_state == 3 && peer.stream.n >= 0;
            },
            "Unsolicited proof fixture did not connect");
        Receive(peer, pump, 41);
        peer.Put(Bytes{1, 0});
        Until(
            pump, [&] { return !login.m_clientLink->m_connection; },
            "Unsolicited proof did not disconnect");
        Require(login.m_logonStatus != 0, "Unsolicited proof authenticated");
    }
    for (unsigned variant : {0u, 1u, 2u}) {
        const bool difficulty = variant == 1;
        Peer peer;
        Client client;
        client.Initialize();
        client.Connect(peer.Address().c_str());
        Until(
            pump,
            [&] {
                peer.Accept();
                return client.m_netState == NS_CONNECTED && peer.stream.n >= 0;
            },
            "World rejection fixture did not connect");
        CDataStore challenge;
        challenge.Put(uint16_t(SMSG_AUTH_CHALLENGE)).Put(uint32_t(difficulty ? 1 : 0));
        if (difficulty) {
            challenge.Put(uint32_t(0));
            for (int i = 0; i < 8; ++i)
                challenge.Put(uint32_t(0));
        }
        Bytes wire{0, uint8_t(challenge.m_size)};
        Append(wire, Wire(challenge));
        if (variant == 2)
            wire = Bytes{0, 2, uint8_t(SMSG_RESUME_COMMS), uint8_t(SMSG_RESUME_COMMS >> 8)};
        peer.Put(wire);
        Until(
            pump, [&] { return client.m_netState == NS_INITIALIZED; },
            "Unsupported world challenge did not disconnect");
        Require(client.events == std::vector<int>({1, 3}),
                "Invalid world challenge reached auth callback");
    }
    // An exception consumes the current payload/reference and leaves a usable queue.
    {
        Client client;
        client.Initialize();
        client.HandleConnect();
        client.SetMessageHandler(
            SMSG_AUTH_RESPONSE,
            +[](void *, NETMESSAGE, uint32_t, CDataStore *) -> int32_t {
                throw std::runtime_error("Authored handler failure");
            },
            nullptr);
        const uint8_t bytes[] = {uint8_t(SMSG_AUTH_RESPONSE), uint8_t(SMSG_AUTH_RESPONSE >> 8), 13};
        client.m_netEventQueue->AddEvent(EVENT_ID_NET_DATA, nullptr, &client, bytes, sizeof(bytes));
        bool rejected = false;
        try {
            pump();
        } catch (const std::runtime_error &) {
            rejected = true;
        }
        Require(rejected && client.m_refCount.m_value == 0 &&
                    client.m_netEventQueue->m_eventQueue.IsEmpty(),
                "Throwing handler retained queue payload/reference");
    }
}
} // namespace
#if defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN)
void CheckNativeTestServerExchange(void (*pump)(),
    const std::function<void(const char*)>& begin,
    const std::function<bool()>& finished, int scenario) {
    Peer peer;
    begin(peer.Address().c_str());
    Until(pump, [&] { peer.Accept(); return peer.stream.n >= 0; }, "Test profile did not connect");
    auto hello = Receive(peer, pump, 41);
    Require(hello[0] == 0 && hello[1] == 8 && hello[2] == 37 && hello[3] == 0 &&
        Bytes(hello.begin()+4, hello.begin()+25) == Hex("576f5700030305343034367800786e4c0053556e65") &&
        hello[33] == 7 && std::string(hello.begin()+34, hello.end()) == "FIXTURE",
        "Test profile did not serialize truthful Linux/x64 12340/enUS identity");
    auto challenge = Challenge();
    if (scenario == 4) { challenge.back() = 4; challenge.push_back(1); }
    Fragment(peer, pump, Bytes(challenge.begin(), challenge.end()-1));
    Require(peer.Read().empty() && !finished(), "Incomplete challenge produced proof/result");
    peer.Put(Bytes{challenge.back()});
    if (scenario == 4) {
        Until(pump, finished, "Unsupported security did not stop sign-in");
        Require(peer.Read().empty(), "Unsupported security emitted a proof");
        return;
    }
    const auto proof = Receive(peer, pump, 75);
    Bytes a(proof.begin()+1, proof.begin()+33), key = ServerSession(a), expected = ClientProof(a,key);
    Require(proof[0] == 1 && proof[73] == 0 && proof[74] == 0 &&
        !memcmp(proof.data()+33, expected.data(),20), "Test profile SRP client proof invalid");
    Bytes unavailable = a;
    unavailable.resize(52, 0);
    Require(!memcmp(proof.data()+53, Hash(unavailable).data(),20),
        "Test profile unavailable-version placeholder changed");
    Require(!finished(), "Login completed before verifying the server proof");
    if (scenario == 2) {
        peer.Put(Bytes{1,9}); // GRUNT_RESULT_BAD_VERSION; never treated as approval
        peer.stream.Close(); // preserve the actual rejection through the close callbacks
        Until(pump, finished, "Server version refusal was ignored");
        return;
    }
    Bytes m2 = a; Append(m2, expected); Append(m2,key); m2=Hash(m2);
    if (scenario == 1) m2[0] ^= 1;
    CDataStore reply;
    reply.Put(uint8_t(1)).Put(uint8_t(0)).PutData(m2.data(),20);
    reply.Put(uint32_t(8)).Put(uint32_t(0)).Put(uint16_t(0));
    auto wire=Wire(reply);
    Fragment(peer,pump,Bytes(wire.begin(),wire.end()-1));
    Require(!finished() && peer.Read().empty(), "Incomplete server proof released a session");
    peer.Put(Bytes{wire.back()});
    if (scenario == 1) {
        Until(pump,finished,"Bad server proof was ignored");
        Require(peer.Read().empty(), "Bad server proof requested realms");
        return;
    }
    Require(Receive(peer,pump,5)==Bytes({0x10,0,0,0,0}), "Original realm request changed");
    peer.Put(RealmPacket(scenario == 3));
    Until(pump,finished,"Test profile realm completion/failure missing");
}
namespace {
void TestServerProfiles(void (*pump)()) {
    for (const char* invalid : {"", "host:", ":3724", "host:0", "host:65536", "host:999999999999", "http://host", "host:abc", "host name", "host:1:2"}) {
        bool rejected=false;
        try { NativeTestServer::ValidateEndpoint(invalid); } catch (const std::invalid_argument&) { rejected=true; }
        Require(rejected,"Invalid external test endpoint accepted");
    }
    for (int scenario=0; scenario<4; ++scenario) {
        Response response;
        NativeTestServer::Login login(0x656e5553);
        login.Init(&response);
        response.onStatus=[&](LOGIN_STATE state) {
            if (state==LOGIN_STATE_CHECKINGVERSIONS) login.ProveUnavailableVersion();
            if (state==LOGIN_STATE_AUTHENTICATED) login.GetRealmList();
        };
        CheckNativeTestServerExchange(pump,[&](const char* endpoint) {
            NativeTestServer::ValidateEndpoint(endpoint);
            login.SetLogonCreds("fixture","authored-password"); login.Logon(endpoint,nullptr);
        },[&] { return response.realms || (!response.states.empty() &&
            (response.states.back()==LOGIN_STATE_FAILED || response.states.back()==LOGIN_STATE_DISCONNECTED)); },scenario);
        Require(!login.m_password,"Test profile retained pending password");
        if (!scenario) {
            Require(response.realms==1 && response.m_realmList.Count()==2 &&
                !strcmp(response.m_realmList[0].m_realmAddress,"127.0.0.1:12345") &&
                response.m_realmList[1].m_versionBuild==12340,
                "Test profile lost original advertised realm data");
        } else Require(!response.realms,"Failed test profile authenticated realms");
    }
    std::cout << "native-test-server-linux-identity-srp-placeholder-realms-failures=passed\n";
}
SARC4Key PeerCipher(const LoginData& data, const char* seedText) {
    const auto seed = Hex(seedText);
    HMAC<SHA1> hmac;
    hmac.Init(seed.data(), seed.size()); hmac.Process(data.m_sessionKey, 40);
    SHA1::Digest digest; hmac.Finish(digest);
    SARC4Key cipher{};
    SARC4PrepareKey(digest.m_data, 20, &cipher);
    uint8_t discard[1024] = {};
    SARC4ProcessBuffer(discard, sizeof(discard), &cipher, &cipher);
    return cipher;
}
void WorldLifecycle(void (*pump)(), LoginData& handoff) {
    for (unsigned scenario = 0; scenario < 8; ++scenario) {
        Peer peer;
        ClientRealmResponseAdapter response;
        ClientConnection client(&response);
        client.SetLoginData(&handoff);
        client.SetSelectedRealm(0, 0, 7);
        client.NetClient::Connect(peer.Address().c_str());
        Until(pump, [&] { peer.Accept(); return client.m_netState == NS_CONNECTED; }, "Realm connect failed");
        CDataStore challenge;
        challenge.Put(uint16_t(SMSG_AUTH_CHALLENGE)).Put(uint32_t(scenario >= 4 ? 160 : 8));
        challenge.Put(uint32_t(0x12345678));
        for (uint32_t i = 0; i < 8; ++i) challenge.Put(i);
        Bytes wire{0, uint8_t(challenge.m_size)}; Append(wire, Wire(challenge));
        Fragment(peer, pump, wire);
        if (scenario >= 4) {
            Until(pump, [&] { return client.m_nativeAuthWork.pending; }, "World work did not start");
            if (scenario == 4) client.Disconnect();
            if (scenario == 5) client.m_nativeAuthWork.started -= 30001;
            if (scenario == 6) peer.stream.Close();
            if (scenario == 7) peer.Put(wire); // duplicate challenge while work is pending
            if (scenario != 4) Until(pump, [&] { return client.m_netState == NS_INITIALIZED; }, "World work did not cancel/timeout/disconnect");
            Require(!client.m_nativeAuthWork.pending && peer.Read().empty(), "Cancelled challenge emitted authentication");
            continue;
        }
        const auto packet = Receive(peer, pump, 74);
        CDataStore auth(const_cast<uint8_t*>(packet.data() + 2), packet.size() - 2);
        uint32_t opcode=0, build=0, serverId=0, type=0, seed=0, region=0, group=0, realm=0, addons=1;
        uint64_t proof=0; uint8_t digest[20]; char account[1280];
        auth.Get(opcode).Get(build).Get(serverId).GetString(account,sizeof(account));
        auth.Get(type).Get(seed).Get(region).Get(group).Get(realm).Get(proof).GetData(digest,20).Get(addons);
        Require(auth.IsRead() && opcode==CMSG_AUTH_SESSION && build==12340 &&
            serverId==uint32_t(handoff.m_loginServerID) && type==uint32_t(handoff.m_loginServerType) &&
            !strcmp(account,handoff.m_account) && region==0 && group==0 && realm==7 && addons==0,
            "Original CMSG_AUTH_SESSION fields changed");
        CDataStore hashInput; hashInput.PutString(account); --hashInput.m_size;
        hashInput.Put(uint32_t(0)).Put(seed).Put(uint32_t(0x12345678)).PutData(handoff.m_sessionKey,40);
        Require(!memcmp(digest,Hash(Wire(hashInput)).data(),20), "World session proof lost verified SRP key");
        CDataStore workInput; workInput.PutString(account); --workInput.m_size;
        for (uint32_t i=0;i<8;++i) workInput.Put(i);
        workInput.Put(proof);
        Require(Hash(Wire(workInput))[0]==0, "Nonzero world proof-of-work is invalid");
        auto sendCipher=PeerCipher(handoff,"cc98ae04e897eaca12ddc09342915357");
        auto receiveCipher=PeerCipher(handoff,"c2b3723cc6aed9b5343c53ee2f4367ce");
        const auto send = [&](uint16_t id, const Bytes& payload) {
            const auto length=payload.size()+2;
            Bytes message{uint8_t(length>>8),uint8_t(length),uint8_t(id),uint8_t(id>>8)};
            SARC4ProcessBuffer(message.data(),4,&sendCipher,&sendCipher);
            Append(message,payload); Fragment(peer,pump,message);
        };
        if (scenario==1) {
            send(SMSG_AUTH_RESPONSE,Bytes{13});
            Until(pump,[&] { return client.m_statusComplete; },"World rejection missing");
            Require(!client.m_authenticated && !client.m_statusResult && client.m_errorCode==13,
                "Server world rejection became success");
            continue;
        }
        send(SMSG_AUTH_RESPONSE,Bytes{27,1,0,0,0,0});
        Until(pump,[&] { return client.m_statusOp==COP_WAIT_QUEUE; },"World queue response missing");
        Require(!client.m_authenticated && client.m_queuePosition==1,"Queue authenticated early");
        send(SMSG_AUTH_RESPONSE,Bytes{12,0,0,0,0,0,0,0,0,0,2});
        Until(pump,[&] { return client.m_authenticated!=0; },"World AUTH_OK missing");
        if (scenario==3) {
            send(SMSG_WARDEN_DATA,Bytes{0});
            Until(pump,[&] { return client.m_netState==NS_INITIALIZED; },"Unsupported Warden was ignored");
            Require(!client.m_authenticated,"Warden disconnect retained authentication");
            continue;
        }
        client.GetCharacterList();
        auto request=Receive(peer,pump,6);
        SARC4ProcessBuffer(request.data(),6,&receiveCipher,&receiveCipher);
        Require(request==Bytes({0,4,uint8_t(CMSG_ENUM_CHARACTERS),uint8_t(CMSG_ENUM_CHARACTERS>>8),0,0}),
            "Encrypted character enumeration changed");
        CDataStore rows;
        rows.Put(uint8_t(1)).Put(uint64_t(123)).PutString("Fixture");
        for (uint8_t value : {1,8,0,0,0,0,0,0,80}) rows.Put(value);
        rows.Put(uint32_t(12)).Put(uint32_t(0)).Put(1.0f).Put(2.0f).Put(3.0f);
        for (unsigned i=0;i<4;++i) rows.Put(uint32_t(0)); // guild, flags, customize, first-login below
        // first-login is uint8; remove the last uint32 above.
        rows.m_size-=4;
        rows.Put(uint8_t(0));
        for (unsigned i=0;i<3;++i) rows.Put(uint32_t(0));
        for (unsigned i=0;i<23;++i) rows.Put(uint32_t(0)).Put(uint8_t(0)).Put(uint32_t(0));
        auto bytes=Wire(rows); if (scenario==2) bytes.pop_back();
        send(SMSG_ENUM_CHARACTERS_RESULT,bytes);
        Until(pump,[&] { return client.m_statusComplete; },"Character response missing");
        Require(bool(client.m_statusResult)==(scenario==0),"Malformed character response became success");
        if (!scenario) Require(client.m_characterList.Count()==1 &&
            !strcmp(client.m_characterList[0].m_name,"Fixture") && client.m_characterList[0].m_experienceLevel==80,
            "Original character row was not delivered");
    }
    std::cout << "native-world-auth-session-pow-encryption-queue-characters-negative-lifetimes=passed\n";
}
}
#endif
void CheckNativeSessions(void (*pump)()) {
    Vectors();
    VersionChecksums();
    LoginData handoff{};
    Authenticate(pump, false, false, &handoff);
    Authenticate(pump, true, false);
    Authenticate(pump, false, true);
    WorldQueue(pump, handoff);
    FailurePaths(pump);
#if defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN)
    TestServerProfiles(pump);
    WorldLifecycle(pump, handoff);
#endif
}
#ifndef WOWDECOMP_SESSION_FIXTURE_EMBEDDED
int main() {
    try {
        for (unsigned cycle = 0; cycle < 3; ++cycle) {
            WowConnection::InitOsNet(nullptr, nullptr, 1, false);
            CheckNativeSessions(+[] {
                WowConnection::s_network->Poll();
                NativeSession::Poll();
            });
            Require(!NetClient::s_clientCount, "Session leaked an owner");
            WowConnection::DestroyOsNet();
        }
        std::cout << "Original Grunt/SRP/realm/NetClient loopback contracts passed (3 cycles)\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
#endif
