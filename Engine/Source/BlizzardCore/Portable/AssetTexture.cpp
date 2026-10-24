#include "AssetBytes.h"
#include <algorithm>

namespace Blizzard { namespace Assets {
using namespace Detail;
namespace {
using Color = std::array<uint8_t, 4>;
Color Rgb565(uint16_t value) {
    unsigned r = value >> 11, g = (value >> 5) & 63, b = value & 31;
    return {static_cast<uint8_t>((r << 3) | (r >> 2)), static_cast<uint8_t>((g << 2) | (g >> 4)),
            static_cast<uint8_t>((b << 3) | (b >> 2)), 255};
}
void Dxt(View data, Image& image, unsigned format, unsigned alphaBits) {
    size_t blockSize = format == 0 ? 8 : 16;
    size_t blocksX = (image.width + 3) / 4, blocksY = (image.height + 3) / 4;
    if (data.size != blocksX * blocksY * blockSize) throw Error("Invalid BLP DXT mip extent");
    for (size_t by = 0; by < blocksY; ++by) for (size_t bx = 0; bx < blocksX; ++bx) {
        auto block = data.Slice((by * blocksX + bx) * blockSize, blockSize);
        size_t colorsOffset = format == 0 ? 0 : 8;
        uint16_t c0 = block.U16(colorsOffset), c1 = block.U16(colorsOffset + 2);
        Color colors[4]{Rgb565(c0), Rgb565(c1), {}, {}};
        if (c0 > c1 || format != 0) {
            for (unsigned c = 0; c < 3; ++c) {
                colors[2][c] = static_cast<uint8_t>((2 * colors[0][c] + colors[1][c]) / 3);
                colors[3][c] = static_cast<uint8_t>((colors[0][c] + 2 * colors[1][c]) / 3);
            }
            colors[2][3] = colors[3][3] = 255;
        } else {
            for (unsigned c = 0; c < 3; ++c) colors[2][c] = static_cast<uint8_t>((colors[0][c] + colors[1][c]) / 2);
            colors[2][3] = 255; colors[3][3] = alphaBits ? 0 : 255;
        }
        uint8_t alphas[8]{};
        uint64_t alphaIndices = 0;
        if (format == 7) {
            alphas[0] = block.data[0]; alphas[1] = block.data[1];
            unsigned last = alphas[0] > alphas[1] ? 7 : 5;
            for (unsigned a = 2; a <= last; ++a)
                alphas[a] = static_cast<uint8_t>(((last + 1 - a) * alphas[0] + (a - 1) * alphas[1]) / last);
            if (last == 5) { alphas[6] = 0; alphas[7] = 255; }
            for (unsigned a = 0; a < 6; ++a) alphaIndices |= uint64_t(block.data[2 + a]) << (8 * a);
        }
        uint32_t indices = block.U32(colorsOffset + 4);
        for (unsigned pixel = 0; pixel < 16; ++pixel) {
            auto color = colors[(indices >> (2 * pixel)) & 3];
            if (format == 1) color[3] = static_cast<uint8_t>(((block.data[pixel / 2] >> ((pixel % 2) * 4)) & 15) * 17);
            if (format == 7) color[3] = alphas[(alphaIndices >> (3 * pixel)) & 7];
            size_t x = bx * 4 + pixel % 4, y = by * 4 + pixel / 4;
            if (x < image.width && y < image.height)
                std::copy(color.begin(), color.end(), image.rgba.begin() + (y * image.width + x) * 4);
        }
    }
}
}
std::vector<Image> DecodeBlp(const Bytes& file, bool baseLevelOnly) {
    View data(file);
    if (data.U32(0) != 0x32504c42 || data.U32(4) != 1 || file.size() < 148)
        throw Error("Expected original BLP2 version 1");
    unsigned encoding = data.data[8], alphaBits = data.data[9], format = data.data[10];
    uint32_t width = data.U32(12), height = data.U32(16);
    if (!width || !height || width > 16384 || height > 16384 || uint64_t(width) * height > 16777216 ||
        (alphaBits != 0 && alphaBits != 1 && alphaBits != 4 && alphaBits != 8)) throw Error("Invalid BLP dimensions/alpha");
    if (encoding < 1 || encoding > 3 || (encoding == 2 && format != 0 && format != 1 && format != 7))
        throw Error("Unsupported BLP encoding/format");
    const bool powerOfTwo = !(width & (width - 1)) && !(height & (height - 1));
    std::vector<Image> images;
    bool ended = false;
    size_t totalPixels = 0;
    for (size_t mip = 0; mip < 16; ++mip) {
        uint32_t offset = data.U32(20 + mip * 4), size = data.U32(84 + mip * 4);
        if (!size) { if (offset) throw Error("BLP empty mip has offset"); ended = true; continue; }
        if (ended || (mip && width == 1 && height == 1)) throw Error("Invalid BLP mip chain");
        if (mip) { width = std::max(1u, width / 2); height = std::max(1u, height / 2); }
        size_t pixels = size_t(width) * height;
        totalPixels += pixels;
        if (totalPixels > 22369632 || offset < (encoding == 1 ? 1172u : 148u)) throw Error("Invalid BLP mip allocation/offset");
        auto source = data.Slice(offset, size);
        if (baseLevelOnly && mip && encoding == 2) {
            const size_t block = format == 0 ? 8 : 16;
            const size_t complete = size_t((width + 3) / 4) * ((height + 3) / 4) * block;
            // Original Glue DXT assets have area-sized thin tails (e.g. 8x2
            // stores one block). No complete RGBA image exists for those levels.
            // Admit only this exact bounded storage form for unrequested tails;
            // explicit mip reads continue to reject it in Dxt below.
            const size_t legacy = std::max(block, pixels * block / 16);
            if (source.size != complete &&
                !(powerOfTwo && (data.data[11] & 0xF) && std::min(width, height) < 4 &&
                  source.size == legacy)) throw Error("Invalid BLP DXT mip extent");
            continue;
        }
        Image image{width, height, Bytes(pixels * 4)};
        if (encoding == 1) {
            auto palette = data.Slice(148, 1024);
            size_t alphaSize = (pixels * alphaBits + 7) / 8;
            if (source.size != pixels + alphaSize) throw Error("Invalid BLP palette mip extent");
            for (size_t i = 0; i < pixels; ++i) {
                size_t index = source.data[i] * 4;
                image.rgba[i * 4] = palette.data[index + 2];
                image.rgba[i * 4 + 1] = palette.data[index + 1];
                image.rgba[i * 4 + 2] = palette.data[index];
                unsigned a = 255;
                if (alphaBits) {
                    a = (source.data[pixels + i * alphaBits / 8] >> ((i * alphaBits) % 8)) & ((1u << alphaBits) - 1);
                    a = a * 255 / ((1u << alphaBits) - 1);
                }
                image.rgba[i * 4 + 3] = static_cast<uint8_t>(a);
            }
        } else if (encoding == 2) Dxt(source, image, format, alphaBits);
        else {
            if (source.size != pixels * 4) throw Error("Invalid BLP raw mip extent");
            for (size_t i = 0; i < pixels; ++i) {
                image.rgba[i * 4] = source.data[i * 4 + 2]; image.rgba[i * 4 + 1] = source.data[i * 4 + 1];
                image.rgba[i * 4 + 2] = source.data[i * 4]; image.rgba[i * 4 + 3] = source.data[i * 4 + 3];
            }
        }
        if (!baseLevelOnly || !mip) images.push_back(std::move(image));
    }
    if (images.empty()) throw Error("BLP has no mip data");
    return images;
}
}}
