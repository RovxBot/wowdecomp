#include "Engine/Source/Gx/Portable/GlRenderTargets.h"
#include "Engine/Source/Gx/Portable/GlBuffers.h"
#include "Engine/Source/Gx/Portable/GlProgram.h"
#include "Engine/Source/Os/Time.h"
#include "Engine/Source/Storm/H/storm.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
using namespace Gx::Native;
namespace {
void Require(bool value, const std::string& message) { if (!value) throw std::runtime_error(message); }
template<class Exception, class F> void Reject(F function) {
    bool rejected = false;
    try { function(); } catch (const Exception&) { rejected = true; }
    Require(rejected, "Invalid native texture operation accepted");
}
ShaderRecord Record(const char* source) {
    ShaderRecord r; r.code.assign(source, source + std::strlen(source) + 1); return r;
}
struct Checks {
    GlContext& context;
#define BASE(name) decltype(&gl##name) name = reinterpret_cast<decltype(name)>(context.Proc("gl" #name))
    BASE(GetError); BASE(GetIntegerv); BASE(GenTextures); BASE(DeleteTextures); BASE(BindTexture);
    BASE(GetTexImage); BASE(GetTexParameteriv); BASE(PixelStorei); BASE(IsTexture);
    BASE(Enable); BASE(Disable); BASE(BlendFunc); BASE(ClearColor); BASE(Clear); BASE(ReadPixels);
#undef BASE
#define EXT(type, name) type name = reinterpret_cast<type>(context.Proc("gl" #name))
    EXT(PFNGLACTIVETEXTUREARBPROC, ActiveTextureARB);
    EXT(PFNGLGENBUFFERSARBPROC, GenBuffersARB);
    EXT(PFNGLBINDBUFFERARBPROC, BindBufferARB);
    EXT(PFNGLBUFFERDATAARBPROC, BufferDataARB);
    EXT(PFNGLDELETEBUFFERSARBPROC, DeleteBuffersARB);
#undef EXT
    explicit Checks(GlContext& owner) : context(owner) {}
    GLint Get(GLenum name) { GLint value = 0; GetIntegerv(name, &value); return value; }
    void Clean() { Require(GetError() == GL_NO_ERROR, "Texture fixture GL error"); }
    std::vector<uint8_t> Read(GlRenderTargets& textures, const GlTexture& texture, uint32_t level = 0, uint32_t face = 0) {
        auto spec = texture.Spec();
        uint32_t w = std::max(1u, spec.width >> level), h = std::max(1u, spec.height >> level);
        textures.Sample(0, texture);
        GLint active = Get(GL_ACTIVE_TEXTURE);
        ActiveTextureARB(GL_TEXTURE0);
        std::vector<uint8_t> pixels(size_t(w) * h * 4);
        PixelStorei(GL_PACK_ALIGNMENT, 1);
        GetTexImage(spec.target == GxTex_CubeMap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + face :
            spec.target == GxTex_Rectangle ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D,
            level, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        ActiveTextureARB(active);
        textures.Sample(0);
        Clean(); return pixels;
    }
    void Pixel(const std::vector<uint8_t>& p, size_t i, int r, int g, int b, int a) {
        const int expected[] = {r, g, b, a};
        for (size_t c = 0; c < 4; ++c)
            Require(std::abs(int(p.at(i * 4 + c)) - expected[c]) <= 1,
                "Texture pixel mismatch channel=" + std::to_string(c) + " actual=" + std::to_string(p.at(i * 4 + c)) +
                " expected=" + std::to_string(expected[c]));
    }
    void Run() {
        GlRenderTargets textures(context), foreign(context);
        // Deliberately hostile unpack state, undersized PBO and foreign active
        // texture binding. CPU upload must neither read that PBO nor leak state.
        GLuint pbo = 0, sentinel = 0;
        GenBuffersARB(1, &pbo); BindBufferARB(GL_PIXEL_UNPACK_BUFFER, pbo);
        BufferDataARB(GL_PIXEL_UNPACK_BUFFER, 8, nullptr, GL_STATIC_DRAW);
        ActiveTextureARB(GL_TEXTURE3); GenTextures(1, &sentinel); BindTexture(GL_TEXTURE_2D, sentinel);
        constexpr GLenum names[] = {GL_UNPACK_ALIGNMENT, GL_UNPACK_ROW_LENGTH, GL_UNPACK_SKIP_PIXELS,
            GL_UNPACK_SKIP_ROWS, GL_UNPACK_IMAGE_HEIGHT, GL_UNPACK_SKIP_IMAGES, GL_UNPACK_SWAP_BYTES, GL_UNPACK_LSB_FIRST};
        constexpr GLint values[] = {8, 7, 2, 1, 9, 1, GL_TRUE, GL_TRUE};
        for (size_t i = 0; i < std::size(names); ++i) PixelStorei(names[i], values[i]);
        bool blockState = SDL_GL_ExtensionSupported("GL_ARB_compressed_texture_pixel_storage");
        constexpr GLenum blocks[] = {GL_UNPACK_COMPRESSED_BLOCK_WIDTH, GL_UNPACK_COMPRESSED_BLOCK_HEIGHT,
            GL_UNPACK_COMPRESSED_BLOCK_DEPTH, GL_UNPACK_COMPRESSED_BLOCK_SIZE};
        constexpr GLint blockValues[] = {4, 4, 1, 8};
        if (blockState) for (size_t i = 0; i < 4; ++i) PixelStorei(blocks[i], blockValues[i]);
        auto restored = [&] {
            Require(Get(GL_ACTIVE_TEXTURE) == GL_TEXTURE3 && GLuint(Get(GL_TEXTURE_BINDING_2D)) == sentinel &&
                GLuint(Get(GL_PIXEL_UNPACK_BUFFER_BINDING)) == pbo, "Texture operation leaked binding state");
            for (size_t i = 0; i < std::size(names); ++i)
                Require(Get(names[i]) == values[i], "Texture operation leaked unpack state");
            if (blockState) for (size_t i = 0; i < 4; ++i)
                Require(Get(blocks[i]) == blockValues[i], "Texture operation leaked compressed block state");
            Clean();
        };
        auto rgba = textures.Create({GxTex_2d, 4, 2, GlTargetFormat::Rgba8, false, 3, false});
        for (uint32_t level = 0; level < 3; ++level) {
            auto zero = Read(textures, rgba, level);
            Require(std::all_of(zero.begin(), zero.end(), [](uint8_t x) { return x == 0; }), "Uninitialized sampled mip");
        }
        std::array<uint8_t, 24> padded{};
        padded[0] = 255; padded[3] = 255; padded[5] = 255; padded[7] = 255;
        padded[14] = 255; padded[15] = 255; padded[16] = padded[17] = padded[19] = 255;
        textures.Upload(rgba, {0, 0, 1, 0, 2, 2}, GxTex_Abgr8888, padded.data(), 20, 12);
        auto pixels = Read(textures, rgba); Pixel(pixels, 1, 255, 0, 0, 255); Pixel(pixels, 2, 0, 255, 0, 255);
        Pixel(pixels, 5, 0, 0, 255, 255); Pixel(pixels, 6, 255, 255, 0, 255); Pixel(pixels, 0, 0, 0, 0, 0);
        const uint8_t bgra[] = {51, 34, 17, 255};
        textures.Upload(rgba, {2, 0, 0, 0, 1, 1}, GxTex_Argb8888, bgra, sizeof(bgra));
        Pixel(Read(textures, rgba, 2), 0, 17, 34, 51, 255); restored();
        for (const auto item : {std::pair<GlTargetFormat, uint32_t>{GlTargetFormat::Rgba4, GxTex_Argb4444},
                {GlTargetFormat::Rgb5A1, GxTex_Argb1555}, {GlTargetFormat::Rgb565, GxTex_Rgb565}}) {
            auto packed = textures.Create({GxTex_2d, 2, 2, item.first, false, 1, false});
            uint16_t texels[6] = {uint16_t(item.second == GxTex_Argb4444 ? 0xFF00 :
                item.second == GxTex_Argb1555 ? 0xFC00 : 0xF800), 0, 0, 0, 0, 0};
            textures.Upload(packed, {0, 0, 1, 1, 1, 1}, item.second, texels, 2);
            Pixel(Read(textures, packed), 3, 255, 0, 0, 255);
            if (item.second == GxTex_Argb4444) {
                texels[0] = 0xF123;
                textures.Upload(packed, {0, 0, 0, 0, 1, 1}, item.second, texels, 2);
                Pixel(Read(textures, packed), 0, 17, 34, 51, 255);
            }
            restored();
        }
        // DXT padded block rows, NPOT partial edge and 1x1 mip tails.
        for (uint32_t code = GxTex_Dxt1; code <= GxTex_Dxt5; ++code) {
            auto format = code == GxTex_Dxt1 ? GlTargetFormat::Dxt1 : code == GxTex_Dxt3 ? GlTargetFormat::Dxt3 : GlTargetFormat::Dxt5;
            auto compressed = textures.Create({GxTex_NonPow2, 6, 5, format, false, 3, false});
            std::array<uint8_t, 16> red{};
            uint32_t offset = code == GxTex_Dxt1 ? 0 : 8, bytes = offset + 8;
            red[offset] = 0; red[offset + 1] = 248; red[offset + 2] = 224; red[offset + 3] = 7;
            if (code == GxTex_Dxt3) std::fill(red.begin(), red.begin() + 8, 255);
            if (code == GxTex_Dxt5) red[0] = 255;
            std::vector<uint8_t> blocksData(bytes * 4 + 8);
            for (uint32_t row = 0; row < 2; ++row) for (uint32_t col = 0; col < 2; ++col)
                std::memcpy(blocksData.data() + row * (bytes * 2 + 8) + col * bytes, red.data(), bytes);
            textures.Upload(compressed, {0, 0, 0, 0, 6, 5}, code, blocksData.data(), blocksData.size(), bytes * 2 + 8);
            Pixel(Read(textures, compressed), 29, 255, 0, 0, 255);
            textures.Upload(compressed, {0, 0, 4, 4, 2, 1}, code, red.data(), bytes);
            textures.Upload(compressed, {2, 0, 0, 0, 1, 1}, code, red.data(), bytes);
            Pixel(Read(textures, compressed, 2), 0, 255, 0, 0, 255); restored();
            Reject<std::invalid_argument>([&] { textures.Upload(compressed, {0, 0, 1, 0, 4, 4}, code, red.data(), bytes); });
            Reject<std::invalid_argument>([&] { textures.Upload(compressed, {0, 0, 0, 0, 2, 4}, code, red.data(), bytes); });
            Reject<std::invalid_argument>([&] { textures.Upload(compressed, {0, 0, 0, 0, 4, 4}, GxTex_Abgr8888, red.data(), bytes); });
            Reject<std::invalid_argument>([&] { textures.Bind({compressed}); });
        }
        auto cube = textures.Create({GxTex_CubeMap, 4, 4, GlTargetFormat::Rgba8, false, 3, false});
        for (uint32_t face = 0; face < 6; ++face) for (uint32_t level = 0; level < 3; ++level) {
            std::vector<uint8_t> texels(size_t(4 >> level) * (4 >> level) * 4, 255);
            for (size_t i = 0; i < texels.size(); i += 4) texels[i] = uint8_t(face * 32 + level);
            textures.Upload(cube, {level, face, 0, 0, 4u >> level, 4u >> level}, GxTex_Abgr8888, texels.data(), texels.size());
            Pixel(Read(textures, cube, level, face), 0, face * 32 + level, 255, 255, 255);
        }
        for (uint32_t filter = 0; filter < GxTexFilters_Last; ++filter) {
            if (filter == GxTex_Anisotropic && !SDL_GL_ExtensionSupported("GL_EXT_texture_filter_anisotropic")) continue;
            textures.Sampling(rgba, {filter, GxTex_Wrap, GxTex_Clamp, 1}); restored();
        }
        auto rectangle = textures.Create({GxTex_Rectangle, 3, 2, GlTargetFormat::Rgba8, false, 1, false});
        textures.Sampling(rectangle, {GxTex_Linear});
        Reject<std::invalid_argument>([&] { textures.Sampling(rectangle, {GxTex_LinearMipLinear}); });
        Reject<std::invalid_argument>([&] { textures.Sampling(rectangle, {0, 1}); });
        Reject<std::invalid_argument>([&] { textures.Sampling(rgba, {UINT32_MAX}); });
        Reject<std::invalid_argument>([&] { textures.Sampling(rgba, {0, 0, 0, NAN}); });
        Reject<std::out_of_range>([&] { textures.Sampling(rgba, {0, 0, 0, 1e30f}); });
        Reject<std::invalid_argument>([&] { foreign.Upload(rgba, {}, GxTex_Abgr8888, bgra, sizeof(bgra)); });
        Reject<std::invalid_argument>([&] { textures.Upload(rgba, {0, 0, 0, 0, 1, 1}, GxTex_R32F, bgra, 4); });
        Reject<std::invalid_argument>([&] { textures.Upload(rgba, {0, 0, 0, 0, 2, 2}, GxTex_Abgr8888, bgra, 4); });
        Reject<std::out_of_range>([&] { textures.Upload(rgba, {0, 0, UINT32_MAX, 0, 1, 1}, GxTex_Abgr8888, bgra, 4); });
        Reject<std::out_of_range>([&] { textures.Upload(rgba, {3, 0, 0, 0, 1, 1}, GxTex_Abgr8888, bgra, 4); });
        Reject<std::out_of_range>([&] { textures.Create({GxTex_2d, UINT32_MAX, UINT32_MAX}); });
        Reject<std::invalid_argument>([&] { textures.Create({UINT32_MAX, 4, 4}); });
        Reject<std::invalid_argument>([&] { textures.Create({GxTex_2d, 2, 2, GlTargetFormat::Rgba8, false, 3}); });
        bool rejected = false;
        std::thread wrong([&] { try { textures.Sampling(rgba, {}); } catch (const std::logic_error&) { rejected = true; } });
        wrong.join(); Require(rejected, "Off-thread texture operation accepted"); restored();
        BindBufferARB(GL_PIXEL_UNPACK_BUFFER, 0); DeleteBuffersARB(1, &pbo);
        BindTexture(GL_TEXTURE_2D, 0); DeleteTextures(1, &sentinel);
        for (auto name : names) PixelStorei(name, name == GL_UNPACK_ALIGNMENT ? 4 : 0);
        if (blockState) for (auto name : blocks) PixelStorei(name, 0);
        ActiveTextureARB(GL_TEXTURE0);
        Atlas(textures);
        std::cout << "gx-texture-packed-formats-strides-mips-cubes-dxt-edges=passed\n"
                     "gx-texture-filter-validation-unpack-pbo-binding-affinity=passed\n";
    }
    void Atlas(GlRenderTargets& textures) {
        auto atlas = textures.Create({GxTex_2d, 2, 2, GlTargetFormat::Rgba4, false, 1, false});
        const uint16_t glyph[] = {0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF};
        textures.Upload(atlas, {0, 0, 0, 0, 2, 2}, GxTex_Argb4444, glyph, sizeof(glyph));
        auto color = textures.Create({GxTex_2d, 16, 16});
        textures.Bind({color});
        Reject<std::logic_error>([&] { textures.Upload(color, {0, 0, 0, 0, 2, 2}, GxTex_Argb4444, glyph, sizeof(glyph)); });
        textures.Sample(0, atlas);
        GLuint name = Get(GL_TEXTURE_BINDING_2D);
        atlas = {}; // Only the sampler retains this allocation now.
        GlProgram vertex(context, ShaderStage::Vertex, Record("!!ARBvp1.0\nMOV result.position, vertex.position;\nMOV result.texcoord[0], vertex.attrib[6];\nEND\n"));
        GlProgram fragment(context, ShaderStage::Fragment, Record("!!ARBfp1.0\nTEX result.color, fragment.texcoord[0], texture[0], 2D;\nEND\n"));
        vertex.Bind(); fragment.Bind();
        GlBuffers buffers(context);
        struct V {float x, y, z, u, v;};
        const V vertices[] = {{-1,-1,0,0,0},{1,-1,0,1,0},{1,1,0,1,1},{-1,-1,0,0,0},{1,1,0,1,1},{-1,1,0,0,1}};
        auto vb = buffers.Create({GxPoolTarget_Vertex, GxPoolUsage_Stream, sizeof(V), 6});
        buffers.Upload(vb, vertices, sizeof(vertices));
        const GlVertexBinding fields[] = {{vb, 0, 4, 0, sizeof(V)}, {vb, 6, 3, 12, sizeof(V)}};
        buffers.VertexFormat(fields, 2);
        Disable(GL_DITHER); Disable(GL_MULTISAMPLE); Disable(GL_DEPTH_TEST); Disable(GL_CULL_FACE); Disable(GL_FRAMEBUFFER_SRGB);
        Enable(GL_VERTEX_PROGRAM_ARB); Enable(GL_FRAGMENT_PROGRAM_ARB); Enable(GL_BLEND);
        BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        ClearColor(0,0,0,0); Clear(GL_COLOR_BUFFER_BIT); buffers.Draw(GxPrim_Triangles, 0, 6);
        std::vector<uint8_t> p(4); ReadPixels(8,8,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p.data());
        Pixel(p, 0, 119,119,119,56);
        textures.Sample(0); Require(!IsTexture(name), "Unbound atlas allocation was retained");
        textures.Bind(); Disable(GL_VERTEX_PROGRAM_ARB); Disable(GL_FRAGMENT_PROGRAM_ARB); Disable(GL_BLEND); Clean();
        std::cout << "gx-synthetic-argb4444-atlas-buffered-blend-and-sampler-lifetime=passed\n";
    }
};
}
int main() {
    try {
        StormRtlInitialize(); OsTimeStartup(OsTimeManager::BestAvailable);
        for (int cycle=0; cycle<3; ++cycle) {
            GlContext context("Gx texture checks",96,64,true);
            std::cout << "gpu=" << context.Info().vendor << ";" << context.Info().renderer << ";" << context.Info().version << ";video=" << context.Info().video << '\n';
            Checks(context).Run(); std::cout << "gx-texture-cycle=" << cycle+1 << ";passed\n";
        }
        OsTimeShutdown(); StormRtlDestroy(); return 0;
    } catch (const std::exception& e) { std::cerr << "Native texture checks failed: " << e.what() << '\n'; return 1; }
}
