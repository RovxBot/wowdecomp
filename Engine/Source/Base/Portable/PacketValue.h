#ifndef WOWDECOMP_NATIVE_PACKET_VALUE_H
#define WOWDECOMP_NATIVE_PACKET_VALUE_H
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>

// Native adaptation of 12340 CDataStore scalar accesses. These are serialized
// little-endian bytes, independent of host alignment, packing and pointer width.
namespace NativePacket {
inline uint32_t End(uint32_t pos, uint32_t bytes) {
    constexpr uint32_t maximum = std::numeric_limits<uint32_t>::max() - 255;
    if (pos > maximum || bytes > maximum - pos)
        throw std::length_error("Packet buffer exceeds its 32-bit address range");
    return pos + bytes;
}
template<class T> void Store(uint8_t* dest, T value) {
    static_assert(std::is_unsigned<T>::value || std::is_same<T,float>::value, "Wire scalar type");
    using Bits = std::conditional_t<sizeof(T)==8,uint64_t,uint32_t>;
    Bits bits = 0;
    if constexpr (std::is_same<T,float>::value) {
        static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559, "IEEE binary32 required");
        std::memcpy(&bits,&value,sizeof(value));
    } else bits = value;
    for (size_t i=0;i<sizeof(T);++i) dest[i]=static_cast<uint8_t>(bits>>(i*8));
}
template<class T> T Load(const uint8_t* source) {
    static_assert(std::is_unsigned<T>::value || std::is_same<T,float>::value, "Wire scalar type");
    using Bits = std::conditional_t<sizeof(T)==8,uint64_t,uint32_t>;
    Bits bits = 0;
    for (size_t i=0;i<sizeof(T);++i) bits|=Bits(source[i])<<(i*8);
    if constexpr (std::is_same<T,float>::value) {
        T value; std::memcpy(&value,&bits,sizeof(value)); return value;
    } else return static_cast<T>(bits);
}
}
#endif
