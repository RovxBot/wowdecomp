// Compile the actual framebuffer, image creation and barrier paths against
// recording Vulkan services. No device, driver or original assets are needed.
#include "Engine/Source/Gx/CGxDeviceVulkan/VkContext.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>

static void check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Vulkan framebuffer contract failed at line %d\n", line);
        std::abort();
    }
}
#define require(value) check((value), __LINE__)
template<class T> static T handle(uintptr_t value) { return reinterpret_cast<T>(value); }

struct VkTexture;
// Only the receiver's used fields are needed; all three method bodies below
// are extracted verbatim from prepared production source, not reimplemented.
class CGxDeviceVulkan {
public:
    VkContext m_vk;
    VkImageAlloc m_offscreenDepth = {};
    VkExtent2D m_offscreenDepthExtent = {};
    uint32_t m_offscreenDepthGeneration = 0;
    VkFramebuffer ITexFramebuffer(VkTexture* texture);
    VkFramebuffer ITexFramebufferBeforeFix(VkTexture* texture);
};

static VkFrame frame = {};
static VkFormat expectedFormat;
static VkImageAspectFlags expectedAspects;
static VkImageMemoryBarrier lastBarrier;
static VkCommandBuffer lastCommand;
static unsigned barriers, rejected, images, views, framebuffers, retiredImages, retiredFramebuffers;
static bool uploadAvailable = true;

VkFrame& VkContext::Frame() { return frame; }
VkCommandBuffer VkContext::UploadCmd() {
    return uploadAvailable ? handle<VkCommandBuffer>(2) : VK_NULL_HANDLE;
}
void VkContext::RetireImage(VkImageAlloc& image) {
    retiredImages += image.image != VK_NULL_HANDLE;
    image = {};
}
void VkContext::Retire(const VkGarbage& garbage) {
    require(garbage.framebuffer && !garbage.image && !garbage.buffer && !garbage.memory);
    ++retiredFramebuffers;
}
int32_t VkHeap::Alloc(VkDevice, const VkMemoryRequirements&, uint32_t type, VkAllocation& out) {
    require(type == 0);
    out.memory = handle<VkDeviceMemory>(3);
    return 1;
}
void VkHeap::Free(const VkAllocation&) { std::abort(); }

static VKAPI_ATTR VkResult VKAPI_CALL createImage(VkDevice, const VkImageCreateInfo* info,
                                                 const VkAllocationCallbacks*, VkImage* out) {
    require(info->format == expectedFormat && info->imageType == VK_IMAGE_TYPE_2D);
    require(info->usage == VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    require(info->mipLevels == 1 && info->arrayLayers == 1 && info->samples == VK_SAMPLE_COUNT_1_BIT);
    *out = handle<VkImage>(10 + ++images);
    return VK_SUCCESS;
}
static VKAPI_ATTR void VKAPI_CALL imageRequirements(VkDevice, VkImage, VkMemoryRequirements* out) {
    *out = {4096, 256, 1};
}
static VKAPI_ATTR VkResult VKAPI_CALL bindImage(VkDevice, VkImage, VkDeviceMemory, VkDeviceSize) {
    return VK_SUCCESS;
}
static VKAPI_ATTR void VKAPI_CALL destroyImage(VkDevice, VkImage, const VkAllocationCallbacks*) {
    std::abort();
}
static VKAPI_ATTR VkResult VKAPI_CALL createView(VkDevice, const VkImageViewCreateInfo* info,
                                                const VkAllocationCallbacks*, VkImageView* out) {
    require(info->format == expectedFormat && info->viewType == VK_IMAGE_VIEW_TYPE_2D);
    // Even combined images retain their valid depth-only attachment view.
    require(info->subresourceRange.aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT);
    require(info->subresourceRange.levelCount == 1 && info->subresourceRange.layerCount == 1);
    *out = handle<VkImageView>(20 + ++views);
    return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL createFramebuffer(VkDevice, const VkFramebufferCreateInfo* info,
                                                       const VkAllocationCallbacks*, VkFramebuffer* out) {
    require(info->renderPass == handle<VkRenderPass>(4));
    require(info->attachmentCount == 2 && info->pAttachments[0] == handle<VkImageView>(5));
    require(info->pAttachments[1] && info->layers == 1);
    *out = handle<VkFramebuffer>(30 + ++framebuffers);
    return VK_SUCCESS;
}
static VKAPI_ATTR void VKAPI_CALL pipelineBarrier(VkCommandBuffer cmd, VkPipelineStageFlags src,
    VkPipelineStageFlags dst, VkDependencyFlags dependencies, uint32_t memoryCount,
    const VkMemoryBarrier*, uint32_t bufferCount, const VkBufferMemoryBarrier*,
    uint32_t imageCount, const VkImageMemoryBarrier* image) {
    require(src == VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT && dst == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    require(!dependencies && !memoryCount && !bufferCount && imageCount == 1);
    require(image->sType == VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER && !image->pNext);
    require(image->oldLayout == VK_IMAGE_LAYOUT_UNDEFINED);
    require(image->newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    require(image->srcAccessMask == 0 && image->dstAccessMask == (VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT));
    require(image->srcQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED && image->dstQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED);
    require(image->subresourceRange.baseMipLevel == 0 && image->subresourceRange.levelCount == 1);
    require(image->subresourceRange.baseArrayLayer == 0 && image->subresourceRange.layerCount == VK_REMAINING_ARRAY_LAYERS);
    lastBarrier = *image;
    lastCommand = cmd;
    ++barriers;
    // Contract oracle: all image aspects must transition together without
    // separateDepthStencilLayouts. Extra aspects on depth-only are invalid too.
    rejected += image->subresourceRange.aspectMask != expectedAspects;
}

#include "RovNativeVulkanMemorySelect.inc"
#include "RovNativeVulkanFramebuffer.inc"

static void exercise(VkFormat format, VkImageAspectFlags aspects, bool recording) {
    expectedFormat = format;
    expectedAspects = aspects;
    frame.recording = recording;
    frame.cmd = handle<VkCommandBuffer>(1);
    CGxDeviceVulkan device;
    device.m_vk.m_localType = 0;
    device.m_vk.m_depthFormat = format;
    device.m_vk.m_passes[VkContext::Pass_Offscreen] = handle<VkRenderPass>(4);
    VkTexture texture = {};
    texture.width = 64;
    texture.height = 32;
    texture.image.view = handle<VkImageView>(5);
    unsigned count = barriers, invalid = rejected, created = views;
    require(device.ITexFramebuffer(&texture));
    require(barriers == count + 1 && rejected == invalid && views == created + 1);
    require(lastCommand == handle<VkCommandBuffer>(recording ? 1 : 2));
    require(lastBarrier.image == device.m_offscreenDepth.image);
    require(device.m_offscreenDepth.format == format && device.m_offscreenDepthGeneration == 1);
    require(device.m_offscreenDepthExtent.width == 64 && device.m_offscreenDepthExtent.height == 32);
    const VkFramebuffer cached = texture.framebuffer;
    require(device.ITexFramebuffer(&texture) == cached && barriers == count + 1);
    const unsigned oldImages = retiredImages, oldFrames = retiredFramebuffers;
    texture.width = 128;
    require(device.ITexFramebuffer(&texture) != cached);
    require(retiredImages == oldImages + 1 && retiredFramebuffers == oldFrames + 1);
    require(device.m_offscreenDepthGeneration == 2 && texture.depthGeneration == 2);
    require(barriers == count + 2 && rejected == invalid);

    // The unchanged donor is a negative control for the reached 03320 defect.
    CGxDeviceVulkan before;
    before.m_vk.m_localType = 0;
    before.m_vk.m_depthFormat = format;
    before.m_vk.m_passes[VkContext::Pass_Offscreen] = handle<VkRenderPass>(4);
    texture.framebuffer = VK_NULL_HANDLE;
    require(before.ITexFramebufferBeforeFix(&texture));
    require(rejected == invalid + (aspects != VK_IMAGE_ASPECT_DEPTH_BIT));

    // Explicit incorrect-mask rejection, including the tempting blanket
    // DEPTH|STENCIL fix on a depth-only image and incomplete combined masks.
    for (VkImageAspectFlags wrong : {0u, uint32_t(VK_IMAGE_ASPECT_COLOR_BIT),
         uint32_t(VK_IMAGE_ASPECT_STENCIL_BIT), uint32_t(aspects ^ VK_IMAGE_ASPECT_STENCIL_BIT),
         uint32_t(aspects | VK_IMAGE_ASPECT_COLOR_BIT)}) {
        invalid = rejected;
        VkContext::Barrier(frame.cmd, device.m_offscreenDepth.image, wrong, 1,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        require(rejected == invalid + 1);
    }
    count = barriers;
    VkContext::Barrier(frame.cmd, device.m_offscreenDepth.image, aspects, 1,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    require(barriers == count); // Preserve the unchanged-layout no-op.

    uploadAvailable = false;
    frame.recording = false;
    const unsigned oldRetired = retiredImages;
    texture.width = 256;
    require(!device.ITexFramebuffer(&texture));
    require(!device.m_offscreenDepth.image && device.m_offscreenDepthExtent.width == 0);
    require(retiredImages == oldRetired + 2 && barriers == count);
    uploadAvailable = true;
}

int main() {
    vkCreateImage = createImage;
    vkGetImageMemoryRequirements = imageRequirements;
    vkBindImageMemory = bindImage;
    vkCreateImageView = createView;
    vkDestroyImage = destroyImage;
    vkCreateFramebuffer = createFramebuffer;
    vkCmdPipelineBarrier = pipelineBarrier;
    for (bool recording : {false, true}) {
        for (VkFormat format : {VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT})
            exercise(format, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, recording);
        for (VkFormat format : {VK_FORMAT_D16_UNORM, VK_FORMAT_X8_D24_UNORM_PACK32, VK_FORMAT_D32_SFLOAT})
            exercise(format, VK_IMAGE_ASPECT_DEPTH_BIT, recording);
    }
    std::puts("native-vulkan-framebuffer=passed: actual framebuffer/image/barrier, combined/depth-only formats and views, invalid-mask rejection, donor negative control, recording/upload, reuse/growth/failure");
}
