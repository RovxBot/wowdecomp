// Controlled loopback protocol peer. Identities, credentials, executable bytes
// and realms below are authored fixtures, never native platform/integrity claims.
#include "WoW/Source/WowServices/GruntLogin.h"
#include "WoW/Source/WowServices/LoginResponse.h"
#include "WoW/Source/WowServices/SRP6_Random.h"
#include "WoW/Source/WowServices/BigInteger.h"
#include "WoW/Source/WowServices/WowConnectionNet.h"
#include "WoW/Source/WowServices/Portable/Session.h"
#include "WoW/Source/Net/NetClient/NetClient.h"
#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>
namespace {
using Bytes = std::vector<uint8_t>;
void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
Bytes Hex(const char* text) {
    Bytes result;
    while (*text) {
        auto digit = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
        result.push_back(uint8_t(digit(text[0]) * 16 + digit(text[1]))); text += 2;
    }
    return result;
}
#include "auth_vectors.h"
Bytes Hash(const Bytes& bytes) {
    Bytes result(20); SHA1_CONTEXT context; SHA1_Init(&context);
    SHA1_Update(&context, bytes.data(), bytes.size()); SHA1_Final(result.data(), &context); return result;
}
void Append(Bytes& out, const Bytes& in) { out.insert(out.end(), in.begin(), in.end()); }
Bytes Wire(CDataStore& msg) { return Bytes(msg.m_data, msg.m_data + msg.m_size); }
struct Big {
    BigInteger n;
    explicit Big(const Bytes& b) : n(BigIntegerFromBytes(b.data(), b.size())) {}
    Big() : n(BigIntegerFromInt(0)) {}
    ~Big() { BigIntegerFree(n); }
    Big(const Big&) = delete;
    Bytes Bytes32() { Bytes b(32); BigIntegerToBytes(n, b.data(), b.size()); return b; }
};
// Server-side algebra uses received A and independently checks M1, version proof
// and M2. Fixed client vectors above also check the arithmetic/hash primitives.
Bytes ServerSession(const Bytes& a) {
    Big N(Prime), A(a), v(Verifier), b(ServerPrivate), u(Hash([&] { Bytes x=a; Append(x,ServerPublic); return x; }())), t, s;
    BigIntegerModExp(t.n,v.n,u.n,N.n); BigIntegerMul(t.n,A.n,t.n);
    BigIntegerModExp(s.n,t.n,b.n,N.n);
    Bytes key(40), secret=s.Bytes32(); SHA1_InterleaveHash(key.data(),secret.data(),secret.size()); return key;
}
Bytes ClientProof(const Bytes& a, const Bytes& key) {
    Bytes x=Hash(Prime), y=Hash(Bytes{7}); for (size_t i=0;i<x.size();++i)x[i]^=y[i];
    Append(x,Hash(Bytes{'F','I','X','T','U','R','E'})); Append(x,Salt); Append(x,a); Append(x,ServerPublic); Append(x,key); return Hash(x);
}
struct Fd { int n=-1; explicit Fd(int v=-1):n(v){} ~Fd(){Close();} void Close(){if(n>=0)close(n);n=-1;} };
struct Peer {
    Fd listener{socket(AF_INET,SOCK_STREAM,0)}, stream;
    uint16_t port=0;
    Peer() {
        sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        Require(listener.n>=0 && !bind(listener.n,reinterpret_cast<sockaddr*>(&addr),sizeof(addr)) && !listen(listener.n,2),"Auth fixture listen failed");
        socklen_t size=sizeof(addr);Require(!getsockname(listener.n,reinterpret_cast<sockaddr*>(&addr),&size),"Auth fixture port missing");
        port=ntohs(addr.sin_port);fcntl(listener.n,F_SETFL,O_NONBLOCK);
    }
    std::string Address(){return "127.0.0.1:"+std::to_string(port);}
    void Accept(){if(stream.n<0){stream.n=accept(listener.n,nullptr,nullptr);if(stream.n>=0)fcntl(stream.n,F_SETFL,O_NONBLOCK);}}
    void Put(const Bytes& b){Require(send(stream.n,b.data(),b.size(),0)==ssize_t(b.size()),"Auth fixture write failed");}
    Bytes Read(){Bytes out;uint8_t b[1024];ssize_t n;while((n=recv(stream.n,b,sizeof(b),0))>0)out.insert(out.end(),b,b+n);return out;}
};
void Until(void(*pump)(),std::function<bool()> done,const char* message){
    auto end=std::chrono::steady_clock::now()+std::chrono::seconds(8);
    while(!done()){Require(std::chrono::steady_clock::now()<end,message);pump();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
}
Bytes Receive(Peer& p,void(*pump)(),size_t size){Bytes out;Until(pump,[&]{Append(out,p.Read());return out.size()>=size;},"Protocol peer receive timed out");Require(out.size()==size,"Unexpected extra protocol bytes");return out;}
void Fragment(Peer& p,void(*pump)(),const Bytes& b){for(uint8_t c:b){p.Put(Bytes{c});pump();}}
struct Response : LoginResponse {
    std::vector<LOGIN_STATE> states;
    unsigned realms=0; uint32_t realmResult=999;
    const char* GetLoginServer() override {return nullptr;}
    int32_t GetLoginServerType() override {return 0;}
    void JoinRealmResult(uint32_t,int32_t,int32_t,int32_t,int32_t) override {throw std::runtime_error("Unexpected realm join");}
    void LoginServerStatus(LOGIN_STATE s,LOGIN_RESULT,const char*,const char*,const char*,uint16_t) override {states.push_back(s);}
    void RealmEnumCallback(uint32_t r) override {++realms;realmResult=r;}
};
struct FixtureLogin : GruntLogin {
    // This test-only callback supplies an authored protocol identity. Production
    // GruntLogin refuses identity selection and UI DefaultServerLogin stays gated.
    void GetLogonMethod() override {
        Grunt::ClientLink::Logon identity{};identity.m_programId=0x576f57;identity.m_processor=0x783836;
        identity.m_os=0x57696e;identity.m_locale=0x656e5553;identity.m_versionMajor=3;identity.m_versionMinor=3;identity.m_versionPatch=5;identity.m_build=12340;
        identity.m_accountName=this->m_username;identity.m_password=this->m_password;
        this->m_clientLink->LogonNewSession(identity);
        memset(this->m_password,0,strlen(this->m_password));SMemFree(this->m_password,__FILE__,__LINE__,0);this->m_password=nullptr;
    }
};
Bytes Challenge(){CDataStore p;p.Put(uint8_t(0)).Put(uint8_t(0)).Put(uint8_t(0));p.PutData(ServerPublic.data(),32);p.Put(uint8_t(1)).Put(uint8_t(7)).Put(uint8_t(32));p.PutData(Prime.data(),32);p.PutData(Salt.data(),32);for(unsigned i=0;i<16;++i)p.Put(uint8_t(0xa0+i));p.Put(uint8_t(0));return Wire(p);}
Bytes RealmPacket(bool malformed=false){
    CDataStore p;p.Put(uint8_t(0x10)).Put(uint16_t(0)).Put(uint32_t(0)).Put(uint16_t(2));
    for(unsigned i=0;i<2;++i){p.Put(uint8_t(1)).Put(uint8_t(i)).Put(uint8_t(i?4:0));p.PutString(i?"Fixture Two":"Fixture/One. ");p.PutString(i?"127.0.0.1:23456":"127.0.0.1:12345");p.Put(1.5f).Put(uint8_t(3)).Put(uint8_t(1)).Put(uint8_t(7+i));if(i)p.Put(uint8_t(3)).Put(uint8_t(3)).Put(uint8_t(5)).Put(uint16_t(12340));}
    p.Put(uint16_t(0x10));p.Set(1,uint16_t(p.m_size-3-(malformed?1:0)));return Wire(p);
}
void Vectors(){
    SRP6_Client client{};client.BeginAuthentication("FIXTURE","AUTHORED-PASSWORD");uint8_t seed[16];for(unsigned i=0;i<16;++i)seed[i]=i;SRP6_Random random(seed,16);uint8_t g=7;
    Require(client.CalculateProof(Prime.data(),32,&g,1,Salt.data(),32,ServerPublic.data(),32,random)==0,"Known SRP vector calculation failed");
    Require(!memcmp(client.m_publicKey,ExpectedA.data(),32)&&!memcmp(client.m_clientProof,ExpectedM.data(),20)&&!memcmp(client.m_sessionKey,ExpectedK.data(),40),"Independent Python SRP vector mismatch");
    Require(client.VerifyServerProof(ExpectedM2.data(),20)==0,"Known server proof rejected");
    Require(client.CalculateProof(nullptr,32,&g,1,Salt.data(),32,ServerPublic.data(),32,random)!=0,"Null SRP modulus accepted");
    Bytes zero(32);Require(client.CalculateProof(Prime.data(),32,&g,1,Salt.data(),32,zero.data(),32,random)!=0,"Zero server public key accepted");
}
void Authenticate(void(*pump)(),bool badProof,bool malformedRealm){
    Peer peer;Response response;FixtureLogin login;login.Init(&response);login.SetLogonCreds("fixture","authored-password");login.Logon(peer.Address().c_str(),nullptr);
    Until(pump,[&]{peer.Accept();return peer.stream.n>=0&&login.m_clientLink->m_state==3;},"Original Grunt logon did not start");
    Bytes hello=Receive(peer,pump,41);Require(hello[0]==0&&hello[1]==8&&hello[2]==37&&hello[3]==0&&hello[33]==7&&std::string(hello.begin()+34,hello.end())=="FIXTURE","Original logon serialization changed");
    auto challenge=Challenge();Fragment(peer,pump,Bytes(challenge.begin(),challenge.end()-1));
    Require(login.m_clientLink->m_state==3,"Partial challenge advanced authentication");peer.Put(Bytes{challenge.back()});
    Until(pump,[&]{return login.m_clientLink->m_state==4;},"Original challenge/security callbacks failed");
    Require(response.states.back()==LOGIN_STATE_CHECKINGVERSIONS&&peer.Read().empty(),"Version prerequisite was bypassed");
    LoginData data{};bool pending=false;try{login.Wait(nullptr,data);}catch(const std::logic_error&){pending=true;}Require(pending,"Blocking native Wait accepted");
    Bytes checksumInput(login.GetVersionChallenge(),login.GetVersionChallenge()+16);Append(checksumInput,Bytes{'a','u','t','h','o','r','e','d','-','e','x','e'});Bytes checksum=Hash(checksumInput);login.ProveVersion(checksum.data());
    Bytes proof=Receive(peer,pump,75),a(proof.begin()+1,proof.begin()+33),key=ServerSession(a),expected=ClientProof(a,key);
    Require(proof[0]==1&&proof[73]==0&&proof[74]==0&&!memcmp(proof.data()+33,expected.data(),20),"Peer rejected SRP client proof");
    Bytes version=a;Append(version,checksum);Require(!memcmp(proof.data()+53,Hash(version).data(),20),"Version proof bytes changed");
    Bytes m2=a;Append(m2,expected);Append(m2,key);m2=Hash(m2);if(badProof)m2[0]^=1;
    CDataStore reply;reply.Put(uint8_t(1)).Put(uint8_t(0));reply.PutData(m2.data(),20);reply.Put(uint32_t(8)).Put(uint32_t(0)).Put(uint16_t(0));Bytes wire=Wire(reply);
    Fragment(peer,pump,Bytes(wire.begin(),wire.end()-2));Require(login.m_logonStatus==-1,"Truncated proof published a result");peer.Put(Bytes(wire.end()-2,wire.end()));
    Until(pump,[&]{return login.m_logonStatus!=-1;},"Original proof callback missing");
    if(badProof){Require(login.m_logonStatus==1&&response.states.back()==LOGIN_STATE_FAILED,"Bad server proof authenticated");return;}
    Require(login.Wait(nullptr,data)&&!memcmp(data.m_sessionKey,key.data(),40)&&response.states.back()==LOGIN_STATE_AUTHENTICATED,"Verified proof/session handoff failed");
    login.GetRealmList();Require(Receive(peer,pump,5)==Bytes({0x10,0,0,0,0}),"Realm request bytes changed");
    auto realms=RealmPacket(malformedRealm);if(!malformedRealm)Append(realms,RealmPacket());peer.Put(realms);
    if(malformedRealm){Until(pump,[&]{return !login.m_clientLink->m_connection;},"Malformed realm length did not disconnect");Require(!response.realms,"Malformed realm reached consumer");return;}
    Until(pump,[&]{return response.realms==2;},"Coalesced realm messages failed");
    Require(response.realmResult==0&&response.m_realmList.Count()==2&&response.m_realmListTrailer==0x10,"Original realm consumer rejected bounded messages");
    auto& r=response.m_realmList[0];Require(!strcmp(r.m_realmName,"Fixture One")&&!strcmp(r.m_realmAddress,"127.0.0.1:12345")&&r.m_realmId==7,"Advertised fixture realm fields changed");
    Require(response.m_realmList[1].m_versionBuild==12340,"Versioned realm extension lost");
}
struct Client : NetClient {
    std::vector<int> events;bool deleteOnConnect=false;unsigned* deleted=nullptr;
    ~Client() override {if(deleted)++*deleted;}
    int32_t HandleConnect() override {events.push_back(1);if(deleteOnConnect){this->DelayedDelete();return 1;}return NetClient::HandleConnect();}
    int32_t HandleDisconnect() override {events.push_back(3);return NetClient::HandleDisconnect();}
    int32_t HandleCantConnect() override {events.push_back(4);return NetClient::HandleCantConnect();}
    void HandleAuthChallenge(AuthenticationChallenge* c) override {Require(c->m_serverSeed==0x12345678&&c->m_proof==0,"World challenge routing changed");events.push_back(2);}
};
void WorldQueue(void(*pump)()){
    Peer peer;Client client;client.Initialize();client.Connect(peer.Address().c_str());Until(pump,[&]{peer.Accept();return client.m_netState==NS_CONNECTED;},"Original NetClient connect queue failed");
    CDataStore challenge;challenge.Put(uint16_t(SMSG_AUTH_CHALLENGE)).Put(uint32_t(0)).Put(uint32_t(0x12345678));for(unsigned i=0;i<8;++i)challenge.Put(uint32_t(i));Bytes wire{0,uint8_t(challenge.m_size)};Append(wire,Wire(challenge));Fragment(peer,pump,wire);Until(pump,[&]{return client.events.size()==2;},"Original NetClient auth event missing");
    Require(client.events==std::vector<int>({1,2})&&client.m_refCount.m_value==0,"Network event ordering/ref ownership failed");
    bool reject=false;client.m_netEventQueue->m_polling=true;try{client.PollEventQueue();}catch(const std::logic_error&){reject=true;}client.m_netEventQueue->m_polling=false;Require(reject,"Queue reentry accepted");
    unsigned messages=0;client.SetMessageHandler(SMSG_AUTH_RESPONSE,+[](void* p,NETMESSAGE,uint32_t,CDataStore* m)->int32_t{uint8_t result=0;m->Get(result);Require(result==13,"Fixture failure response changed");++*static_cast<unsigned*>(p);return 1;},&messages);
    peer.Put(Bytes{0,3,uint8_t(SMSG_AUTH_RESPONSE),uint8_t(SMSG_AUTH_RESPONSE>>8),13});Until(pump,[&]{return messages==1;},"Original message handler not dispatched");
    peer.stream.Close();Until(pump,[&]{return client.m_netState==NS_INITIALIZED;},"Original NetClient disconnect missing");Require(client.events.back()==3,"Disconnect event lost");
    client.Destroy();Require(WowConnection::s_network!=nullptr,"Session destroyed Event transport");
    // Queued payload disposal, cancellation and deletion requested by a callback.
    unsigned deleted=0;auto deferred=new Client;deferred->deleted=&deleted;deferred->Initialize();deferred->deleteOnConnect=true;
    deferred->m_netEventQueue->AddEvent(EVENT_ID_NET_CONNECT,nullptr,deferred,nullptr,0);pump();pump();Require(deleted==1,"DelayedDelete did not release queue/client");
    Client cancelled;cancelled.Initialize();uint8_t byte=0;cancelled.m_netEventQueue->AddEvent(EVENT_ID_NET_DATA,nullptr,&cancelled,&byte,1);cancelled.Destroy();Require(cancelled.m_refCount.m_value==0,"Cancelled event retained a reference");
}
}
void CheckNativeSessions(void(*pump)()){
    Vectors();Authenticate(pump,false,false);Authenticate(pump,true,false);Authenticate(pump,false,true);WorldQueue(pump);
}
#ifndef WOWDECOMP_SESSION_FIXTURE_EMBEDDED
int main(){try{for(unsigned cycle=0;cycle<3;++cycle){WowConnection::InitOsNet(nullptr,nullptr,1,false);CheckNativeSessions(+[]{WowConnection::s_network->Poll();NativeSession::Poll();});Require(!NetClient::s_clientCount,"Session leaked an owner");WowConnection::DestroyOsNet();}std::cout<<"Original Grunt/SRP/realm/NetClient loopback contracts passed (3 cycles)\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
#endif
