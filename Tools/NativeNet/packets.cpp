#include "Engine/Source/Base/CDataStore.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Util/SHA1.h"
#include "WoW/Source/WowServices/HMAC.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
void Require(bool ok,const char* text) { if (!ok) throw std::runtime_error(text); }
template<class E,class F> void Reject(F function) {
    bool rejected=false;try {function();}catch(const E&){rejected=true;}
    Require(rejected,"Invalid packet operation accepted");
}
void Buffers() {
    const uint8_t wire[]={0xA5,0x34,0x12,0xEF,0xCD,0xAB,0x89,
        0xEF,0xCD,0xAB,0x89,0x67,0x45,0x23,0x01,0,0,0x20,0xC0};
    CDataStore packet;
    packet.Put(uint8_t(0xA5)).Put(uint16_t(0x1234)).Put(uint32_t(0x89ABCDEF))
        .Put(uint64_t(0x0123456789ABCDEF)).Put(-2.5f);
    Require(packet.m_size==sizeof(wire) && !std::memcmp(packet.m_data,wire,sizeof(wire)),"12340 wire bytes changed");
    std::vector<uint8_t> unaligned(sizeof(wire)+1);std::memcpy(unaligned.data()+1,wire,sizeof(wire));
    CDataStore view(unaligned.data()+1,sizeof(wire));
    uint8_t a=0;uint16_t b=0;uint32_t c=0;uint64_t d=0;float f=0;
    view.Get(a).Get(b).Get(c).Get(d).Get(f);
    Require(a==0xA5 && b==0x1234 && c==0x89ABCDEF && d==0x0123456789ABCDEF && f==-2.5f && view.IsRead(),"Unaligned packet scalars changed");
    view.Set(1,uint16_t(0x9876));Require(unaligned[2]==0x76 && unaligned[3]==0x98,"Unaligned packet Set changed");
    Reject<std::out_of_range>([&]{view.FetchWrite(sizeof(wire),1,nullptr,0);});
    Require(!view.FetchRead(UINT32_MAX,2),"Read range wrapped at 32 bits");
    Reject<std::length_error>([&]{packet.FetchWrite(UINT32_MAX,1,nullptr,0);});
    uint8_t words[]={0,0x44,0x33,0x22,0x11,0xDD,0xCC,0xBB,0xAA};
    CDataStore array(words+1,8);uint32_t result[2]{};array.GetArray(result,2);
    Require(result[0]==0x11223344 && result[1]==0xAABBCCDD && array.IsRead(),"Unaligned word array changed");
    array.m_read=0;uint64_t untouched=0x1234;array.m_size=4;array.Get(untouched);
    Require(untouched==0x1234 && array.m_read==5,"Truncated scalar read changed output or failure cursor");
    array.m_read=0;array.GetArray(result,UINT32_MAX);
    Require(array.m_read==5,"Word array byte count wrapped");
    CDataStore self;std::vector<uint8_t> original(200);
    for(size_t i=0;i<original.size();++i)original[i]=uint8_t(i);
    self.PutData(original.data(),original.size());self.PutData(self.m_data,self.m_size);
    Require(self.m_size==400 && !std::memcmp(self.m_data,original.data(),200) && !std::memcmp(self.m_data+200,original.data(),200),"Self append lost source across allocation");
    CDataStoreCache<3> cached;cached.Put(uint8_t(1)).Put(uint64_t(0xFEDCBA9876543210));cached.Finalize();
    cached.Get(a).Get(d);Require(a==1 && d==0xFEDCBA9876543210,"Cached packet growth changed bytes");
    CDataStoreCache<16> detached;detached.PutString("abc");void* owned=nullptr;uint32_t size=0;
    detached.DetachBuffer(&owned,&size,nullptr);Require(size==4 && !std::memcmp(owned,"abc",4),"Cache detach did not retain owned bytes");
    SMemFree(owned,__FILE__,__LINE__,0);
    CDataStore empty;empty.Finalize();void* nothing=reinterpret_cast<void*>(1);empty.GetDataInSitu(nothing,0);
    Require(!nothing && empty.IsRead(),"Empty packet view is invalid");
    view.Reset();Require(view.m_data==nullptr && !view.m_size,"Borrowed reset retained source ownership");
}
void Cipher() {
    const uint8_t expected[]={0xBB,0xF3,0x16,0xE8,0xD9,0x40,0xAF,0x0A,0xD3};
    const uint8_t key[]={'K','e','y'};
    std::vector<uint8_t> bytes(10);std::memcpy(bytes.data()+1,"Plaintext",9);
    SARC4Key initial{},state{};SARC4PrepareKey(key,3,&initial);state=initial;
    SARC4ProcessBuffer(bytes.data()+1,3,&state,&state);SARC4ProcessBuffer(bytes.data()+4,6,&state,&state);
    Require(!std::memcmp(bytes.data()+1,expected,9),"Unaligned/chunked ARC4 wire vector changed");
    std::memcpy(bytes.data()+1,"Plaintext",9);SARC4Key copy{};
    SARC4ProcessBuffer(bytes.data()+1,9,&initial,&copy);
    Require(!std::memcmp(bytes.data()+1,expected,9),"Copied ARC4 state changed output");
    SARC4Key saved=initial;SARC4PrepareKey(key,0,&initial);
    Require(!std::memcmp(&saved,&initial,sizeof(saved)) && SErrGetLastError()==STORM_ERROR_INVALID_PARAMETER,"Empty cipher key was accepted");
}
void Digests() {
    const uint8_t expected[]={0xA9,0x99,0x3E,0x36,0x47,0x06,0x81,0x6A,0xBA,0x3E,0x25,0x71,0x78,0x50,0xC2,0x6C,0x9C,0xD0,0xD8,0x9D};
    SHA1 hash;SHA1::Digest digest;hash.Prepare();hash.Process("a",1);hash.Process("bc",2);hash.Finish(digest);
    Require(!std::memcmp(&digest,expected,20),"12340 SHA1 vector changed");
    SHA1_CONTEXT context;uint8_t result[20];SHA1_Init(&context);SHA1_Update(&context,reinterpret_cast<const uint8_t*>("abc"),3);SHA1_Final(result,&context);
    Require(!std::memcmp(result,expected,20),"12340 SHA1 context vector changed");
    std::array<uint8_t,20> key{};key.fill(0x0B);HMAC<SHA1> hmac;
    const uint8_t expectedHmac[]={0xB6,0x17,0x31,0x86,0x55,0x05,0x72,0x64,0xE2,0x8B,0xC0,0xB6,0xFB,0x37,0x8C,0x8E,0xF1,0x46,0xBE,0x00};
    hmac.Init(key.data(),key.size());hmac.Process("Hi There",8);hmac.Finish(digest);
    Require(!std::memcmp(&digest,expectedHmac,20),"12340 authentication HMAC vector changed");
    // An offset full block exercises both compression APIs without aligned input.
    std::array<uint8_t,66> block{};for(size_t i=0;i<65;++i)block[i+1]=uint8_t(i*13);
    auto original=block;SHA1 classHash;classHash.Prepare();classHash.Process(block.data()+1,65);classHash.Finish(digest);
    SHA1_Init(&context);SHA1_Update(&context,block.data()+1,65);SHA1_Final(result,&context);
    Require(!std::memcmp(&digest,result,20) && block==original,"Unaligned SHA1 compression changed input or API parity");
}
}
int main() {
    try {
        StormRtlInitialize();
        for(int i=0;i<3;++i) { Buffers();Cipher();Digests();std::cout<<"12340-packet-buffers-cipher-digests-cycle="<<i+1<<";passed\n"; }
        StormRtlDestroy();return 0;
    } catch(const std::exception& error) {std::cerr<<"Native packet check failed: "<<error.what()<<'\n';return 1;}
}
