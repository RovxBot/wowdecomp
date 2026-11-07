// Compile the actual adapted VkContext selectors and image fallback. Vulkan
// allocation is deliberately stopped at the heap boundary: no GPU is needed.
#include "Engine/Source/Gx/CGxDeviceVulkan/VkContext.h"
#include <cstdio>
#include <cstdlib>

static void check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Vulkan memory selection failed at line %d\n", line);
        std::abort();
    }
}
#define require(value) check((value), __LINE__)

static VkMemoryRequirements requirements;
static uint32_t selectedType = UINT32_MAX;
static unsigned allocations = 0, destroys = 0;

static VKAPI_ATTR VkResult VKAPI_CALL createImage(VkDevice, const VkImageCreateInfo*,
                                                 const VkAllocationCallbacks*, VkImage* out) {
    *out = reinterpret_cast<VkImage>(uintptr_t(1));
    return VK_SUCCESS;
}
static VKAPI_ATTR void VKAPI_CALL imageRequirements(VkDevice, VkImage, VkMemoryRequirements* out) {
    *out = requirements;
}
static VKAPI_ATTR void VKAPI_CALL destroyImage(VkDevice, VkImage, const VkAllocationCallbacks*) {
    ++destroys;
}
int32_t VkHeap::Alloc(VkDevice, const VkMemoryRequirements&, uint32_t type, VkAllocation&) {
    selectedType = type;
    ++allocations;
    return 0; // Observe selection, then exercise the real allocation-failure exit.
}
void VkHeap::Free(const VkAllocation&) {
    std::abort(); // No successful allocation exists in this fixture.
}

#include "RovNativeVulkanMemorySelect.inc"

static void imageType(VkContext& context, uint32_t bits, uint32_t expected) {
    selectedType = UINT32_MAX;
    const unsigned before = allocations, freed = destroys;
    requirements = {4096, 256, bits};
    VkImageAlloc out = {};
    VkImageCreateInfo info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    require(!context.CreateImage(info, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_VIEW_TYPE_2D, {}, out));
    require(selectedType == expected);
    require(allocations == before + (expected != UINT32_MAX));
    require(destroys == freed + 1 && out.image == VK_NULL_HANDLE);
}

int main() {
    vkCreateImage = createImage;
    vkGetImageMemoryRequirements = imageRequirements;
    vkDestroyImage = destroyImage;
    constexpr auto local = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    constexpr auto host = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    constexpr auto restricted = VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD |
                                VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD;
    VkContext context;
    auto& memory = context.m_memoryProperties;
    memory.memoryHeapCount = 2;
    memory.memoryHeaps[0].size = 8ull << 30;
    memory.memoryHeaps[1].size = 16ull << 30;
    memory.memoryTypeCount = 4;
    memory.memoryTypes[0] = {local, 0};
    memory.memoryTypes[1] = {host, 1};
    memory.memoryTypes[2] = {local | host, 0};
    // The reached RADV topology: an unsupported duplicate follows ordinary
    // local memory on the same heap. The old >= preference selected it.
    memory.memoryTypes[3] = {local | restricted, 0};
    context.ChooseHeaps();
    require(context.m_localType == 0 && context.m_uploadType == 1 && context.m_rebarType == 2);
    require(context.FindMemoryType(1u << 3, local) == UINT32_MAX);
    require(context.FindMemoryType((1u << 2) | (1u << 3), local) == 2);
    imageType(context, 1u, 0); // Preferred non-visible local allocation.
    imageType(context, 1u << 3, UINT32_MAX); // Never allocate the forbidden type.
    imageType(context, (1u << 2) | (1u << 3), 2); // Host-visible local fallback.

    // Restricted types appearing first must not win generic/image fallback
    // or host/ReBAR selection either; the restriction is not a tie-break.
    memory.memoryTypeCount = 6;
    memory.memoryTypes[0] = {local | restricted, 0};
    memory.memoryTypes[1] = {local, 0};
    memory.memoryTypes[2] = {host | restricted, 1};
    memory.memoryTypes[3] = {host, 1};
    memory.memoryTypes[4] = {local | host | restricted, 0};
    memory.memoryTypes[5] = {local | host, 0};
    context.ChooseHeaps();
    require(context.m_localType == 1 && context.m_uploadType == 3 && context.m_rebarType == 5);
    require(context.FindMemoryType(~0u, local) == 1);
    require(context.FindMemoryType(~0u, host) == 3);
    require(context.FindMemoryType(~0u, restricted) == UINT32_MAX);
    context.m_localType = UINT32_MAX; // Force CreateImage's non-visible scan.
    imageType(context, 3u, 1);
    imageType(context, 1u, UINT32_MAX);

    // Preserve small-BAR/UMA host-coherent fallback and legal tie ordering.
    memory.memoryTypeCount = 2;
    memory.memoryHeaps[0].size = 256ull << 20;
    memory.memoryTypes[0] = {local | host, 0};
    memory.memoryTypes[1] = {local | host | restricted, 0};
    context.ChooseHeaps();
    require(context.m_rebarType == UINT32_MAX && context.m_localType == UINT32_MAX);
    require(context.m_uploadType == 0);
    memory.memoryTypes[0] = {local, 0};
    memory.memoryTypes[1] = {local, 0};
    context.ChooseHeaps();
    require(context.m_localType == 1 && context.m_uploadType == UINT32_MAX);
    memory.memoryTypes[0] = {local | restricted, 0};
    memory.memoryTypes[1] = {host | restricted, 1};
    context.ChooseHeaps();
    require(context.m_localType == UINT32_MAX && context.m_uploadType == UINT32_MAX &&
            context.m_rebarType == UINT32_MAX);
    require(context.FindMemoryType(0, 0) == UINT32_MAX);
    memory.memoryTypeCount = 32;
    memory.memoryTypes[31] = {local, 0};
    require(context.FindMemoryType(1u << 31, local) == 31);
    memory.memoryTypeCount = 0;
    context.ChooseHeaps();
    require(context.FindMemoryType(~0u, 0) == UINT32_MAX);
    std::puts("native-vulkan-memory=passed: actual preferred/fallback/image selection, restricted AMD types, legal ReBAR/UMA/small-BAR/ties and unavailable types");
}
