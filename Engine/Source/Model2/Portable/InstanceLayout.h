#ifndef WOWDECOMP_NATIVE_MODEL_INSTANCE_LAYOUT_H
#define WOWDECOMP_NATIVE_MODEL_INSTANCE_LAYOUT_H
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace Model2Native {
// Offsets within the existing single SMem-owned model instance allocation.
// Preserve original 32-bit byte bounds while honoring native member alignment.
class InstanceLayout {
    size_t bytes = 0;
public:
    template<class T> size_t Reserve(uint32_t count) {
        static_assert(alignof(T) <= alignof(std::max_align_t), "SMem alignment contract");
        if (!count) return bytes;
        const size_t padding = (alignof(T) - bytes % alignof(T)) % alignof(T);
        if (padding > UINT32_MAX - bytes || count > (UINT32_MAX - bytes - padding) / sizeof(T))
            throw std::length_error("Native model instance allocation exceeds original byte range");
        const auto offset = bytes + padding;
        bytes = offset + size_t(count) * sizeof(T);
        return offset;
    }
    size_t Size() const { return bytes; }
};
}
#endif
