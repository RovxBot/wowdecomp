#include "Engine/Source/Gx/Portable/GlBuffers.h"
#include "Engine/Source/Gx/Portable/GlProgram.h"
#include "Engine/Source/Gx/Portable/GlRenderTargets.h"
#include "Engine/Source/Os/Time.h"
#include "Engine/Source/Storm/H/storm.h"
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace Gx::Native;
namespace {
void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class Exception, class F> void Reject(F function) {
    bool rejected = false;
    try { function(); } catch (const Exception&) { rejected = true; }
    Require(rejected, "Invalid Gx buffer operation accepted");
}
ShaderRecord Record(const char* source) {
    ShaderRecord result;
    result.code.assign(source, source + std::strlen(source) + 1);
    return result;
}
struct Vertex { float p[3]; uint8_t color[4]; float uv[2]; };
static_assert(sizeof(Vertex) == 24, "12340 PCT size");
const Vertex triangle[] = {
    {{-0.75f, -0.75f, 0}, {64, 128, 192, 255}, {0.25f, 0.75f}},
    {{ 0.75f, -0.75f, 0}, {64, 128, 192, 255}, {0.25f, 0.75f}},
    {{ 0.00f,  0.75f, 0}, {64, 128, 192, 255}, {0.25f, 0.75f}}};
struct Checks {
    GlContext& context;
#define BASE(name) decltype(&gl##name) name = reinterpret_cast<decltype(name)>(context.Proc("gl" #name))
    BASE(GetError); BASE(GetIntegerv); BASE(ClearColor); BASE(Clear);
    BASE(ReadPixels); BASE(Enable); BASE(Disable); BASE(Flush);
#undef BASE
#define EXT(type, name) type name = reinterpret_cast<type>(context.Proc("gl" #name))
    EXT(PFNGLGENBUFFERSARBPROC, GenBuffersARB);
    EXT(PFNGLDELETEBUFFERSARBPROC, DeleteBuffersARB);
    EXT(PFNGLBINDBUFFERARBPROC, BindBufferARB);
    EXT(PFNGLGENVERTEXARRAYSPROC, GenVertexArrays);
    EXT(PFNGLDELETEVERTEXARRAYSPROC, DeleteVertexArrays);
    EXT(PFNGLBINDVERTEXARRAYPROC, BindVertexArray);
#undef EXT
    explicit Checks(GlContext& owner) : context(owner) {}
    GLint Get(GLenum name) { GLint value = 0; GetIntegerv(name, &value); return value; }
    void Clean() { Require(GetError() == GL_NO_ERROR, "Gx buffer fixture GL error"); }
    void Pixel(int r, int g, int b) {
        std::array<uint8_t, 4> pixel{};
        ReadPixels(16, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        if (pixel[0] != r || pixel[1] != g || pixel[2] != b)
            throw std::runtime_error("Buffered Gx pixel mismatch: actual=" + std::to_string(pixel[0]) + "," +
                std::to_string(pixel[1]) + "," + std::to_string(pixel[2]) + ";expected=" +
                std::to_string(r) + "," + std::to_string(g) + "," + std::to_string(b));
        Clean();
    }
    void Run() {
        GlRenderTargets targets(context);
        auto color = targets.Create({GxTex_2d, 32, 32});
        targets.Bind({color});
        Disable(GL_DITHER); Disable(GL_MULTISAMPLE); Disable(GL_FRAMEBUFFER_SRGB);
        Disable(GL_DEPTH_TEST); Disable(GL_CULL_FACE); Disable(GL_BLEND);
        ClearColor(0, 0, 0, 1); Clear(GL_COLOR_BUFFER_BIT);
        GlProgram vertex(context, ShaderStage::Vertex, Record(
            "!!ARBvp1.0\nMOV result.position, vertex.position;\n"
            "MOV result.color, vertex.attrib[4];\nEND\n"));
        GlProgram fragment(context, ShaderStage::Fragment, Record(
            "!!ARBfp1.0\nMOV result.color, fragment.color;\nEND\n"));
        vertex.Bind(); fragment.Bind();
        Enable(GL_VERTEX_PROGRAM_ARB); Enable(GL_FRAGMENT_PROGRAM_ARB);

        // Install unrelated caller state and verify it survives every operation,
        // including element updates (element binding is part of the VAO).
        GLuint foreignVao = 0, foreignBuffers[2]{};
        GenVertexArrays(1, &foreignVao); BindVertexArray(foreignVao);
        GenBuffersARB(2, foreignBuffers);
        BindBufferARB(GL_ARRAY_BUFFER_ARB, foreignBuffers[0]);
        BindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, foreignBuffers[1]);
        auto bindings = [&] {
            Require(GLuint(Get(GL_VERTEX_ARRAY_BINDING)) == foreignVao &&
                GLuint(Get(GL_ARRAY_BUFFER_BINDING)) == foreignBuffers[0] &&
                GLuint(Get(GL_ELEMENT_ARRAY_BUFFER_BINDING)) == foreignBuffers[1],
                "Native Gx buffer operation changed caller bindings");
            Clean();
        };
        {
            GlBuffers buffers(context), foreign(context);
            auto vb = buffers.Create({GxPoolTarget_Vertex, GxPoolUsage_Stream, 24, 4});
            auto ib = buffers.Create({GxPoolTarget_Index, GxPoolUsage_Dynamic, 2, 4});
            bindings();
            Require(vb.Spec().itemSize == 24 && vb.Spec().itemCount == 4, "Buffer metadata mismatch");
            GlVertexBinding fields[] = {{vb, 0, 4, 0, 24}, {vb, 4, 0, 12, 24}, {vb, 6, 3, 16, 24}};
            buffers.VertexFormat(fields, 3); buffers.IndexBuffer(ib);
            for (auto& field : fields) field.buffer = {};
            Reject<std::logic_error>([&] { buffers.Draw(GxPrim_Triangles, 0, 3); });
            // A hole in the index upload is rejected before GL; uploading each
            // adjacent region merges coverage and permits the same draw.
            const uint16_t indices[] = {65535, 1, 2, 3};
            buffers.Upload(ib, indices + 1, 2, 2);
            Reject<std::logic_error>([&] { buffers.Draw(GxPrim_Triangles, 1, 3, true); });
            buffers.Upload(ib, indices + 2, 4, 4);
            buffers.Upload(vb, triangle, sizeof(triangle), sizeof(Vertex));
            buffers.Draw(GxPrim_Triangles, 1, 3); Pixel(64, 128, 192);
            Clear(GL_COLOR_BUFFER_BIT);
            buffers.Draw(GxPrim_Triangles, 1, 3, true); Pixel(64, 128, 192);
            bindings();
            // Uploading the poison index makes it defined but out of bounds.
            buffers.Upload(ib, indices, 2);
            Reject<std::out_of_range>([&] { buffers.Draw(GxPrim_Triangles, 0, 3, true); });
            Reject<std::out_of_range>([&] { buffers.Draw(GxPrim_Triangles, UINT32_MAX, 3, true); });
            Reject<std::out_of_range>([&] { buffers.Draw(GxPrim_Triangles, UINT32_MAX, 3); });
            Reject<std::out_of_range>([&] { buffers.Draw(UINT32_MAX, 0, 3); });
            Reject<std::out_of_range>([&] { buffers.Draw(GxPrim_Triangles, 0, UINT32_MAX); });
            buffers.Draw(GxPrim_Triangles, UINT32_MAX, 0);

            // Abandoned writes preserve uploaded data; partial/whole commits
            // use byte counts, and a live write prevents upload/draw/relock.
            {
                auto write = buffers.Lock(vb);
                std::memset(write.Data(), 0, write.Size());
                Reject<std::logic_error>([&] { buffers.Lock(vb); });
                Reject<std::logic_error>([&] { buffers.Upload(vb, triangle, sizeof(triangle)); });
                Reject<std::logic_error>([&] { buffers.Draw(GxPrim_Triangles, 1, 3); });
            }
            Clear(GL_COLOR_BUFFER_BIT);
            buffers.Draw(GxPrim_Triangles, 1, 3); Pixel(64, 128, 192);
            {
                auto write = buffers.Lock(vb);
                std::memcpy(write.Data(), triangle, sizeof(triangle));
                Reject<std::out_of_range>([&] { write.Commit(write.Size() + 1); });
                write.Commit(sizeof(triangle));
                Reject<std::logic_error>([&] { write.Data(); });
            }
            Clear(GL_COLOR_BUFFER_BIT);
            buffers.Draw(GxPrim_Triangles, 0, 3); Pixel(64, 128, 192);
            {
                auto write = buffers.Lock(ib);
                const uint16_t replacement[] = {0, 1, 2, 0};
                std::memcpy(write.Data(), replacement, sizeof(replacement));
                write.Commit();
            }
            // The format and index owner retain allocations after handles go.
            vb = {}; ib = {};
            Clear(GL_COLOR_BUFFER_BIT);
            buffers.Draw(GxPrim_Triangles, 0, 3, true); Pixel(64, 128, 192);
            bindings();

            auto fresh = buffers.Create({GxPoolTarget_Vertex, GxPoolUsage_Static, 12, 3});
            const GlVertexBinding position{fresh, 0, 4, 0, 12};
            auto invalid = position;
            invalid.attribute = UINT32_MAX;
            Reject<std::invalid_argument>([&] { buffers.VertexFormat(&invalid, 1); });
            invalid = position; invalid.type = 7;
            Reject<std::invalid_argument>([&] { buffers.VertexFormat(&invalid, 1); });
            invalid = position; invalid.stride = 4;
            Reject<std::out_of_range>([&] { buffers.VertexFormat(&invalid, 1); });
            invalid = position; invalid.offset = UINT32_MAX;
            Reject<std::out_of_range>([&] { buffers.VertexFormat(&invalid, 1); });
            const GlVertexBinding duplicate[] = {position, position};
            Reject<std::invalid_argument>([&] { buffers.VertexFormat(duplicate, 2); });
            Reject<std::invalid_argument>([&] { buffers.VertexFormat(nullptr, 1); });
            Reject<std::invalid_argument>([&] { foreign.VertexFormat(&position, 1); });
            Reject<std::invalid_argument>([&] { foreign.Upload(fresh, triangle, 12); });
            Reject<std::invalid_argument>([&] { foreign.Lock(fresh); });
            Reject<std::invalid_argument>([&] { buffers.IndexBuffer(fresh); });
            Reject<std::out_of_range>([&] { buffers.Upload(fresh, triangle, 12, UINT32_MAX); });
            Reject<std::invalid_argument>([&] { buffers.Upload(fresh, nullptr, 12); });
            Reject<std::invalid_argument>([&] { buffers.Create({UINT32_MAX, 0, 12, 3}); });
            Reject<std::invalid_argument>([&] { buffers.Create({0, UINT32_MAX, 12, 3}); });
            Reject<std::invalid_argument>([&] { buffers.Create({1, 0, 4, 3}); });
            Reject<std::invalid_argument>([&] { buffers.Create({0, 0, 0, 3}); });
            Reject<std::out_of_range>([&] { buffers.Create({0, 0, UINT32_MAX, UINT32_MAX}); });
            buffers.Upload(fresh, nullptr, 0, 36);
            // Rejected formats did not replace the last valid, retained one.
            Clear(GL_COLOR_BUFFER_BIT);
            buffers.Draw(GxPrim_Triangles, 0, 3, true); Pixel(64, 128, 192);
            bool rejected = false;
            std::thread wrong([&] {
                try { buffers.Lock(fresh); } catch (const std::logic_error&) { rejected = true; }
            });
            wrong.join(); Require(rejected, "Off-thread buffer operation accepted");
            // Every original attribute type code, separate streams and tightly
            // packed zero strides. Asymmetric bytes make accidental color
            // swizzling and normalization changes observable.
            GlProgram typeVertex(context, ShaderStage::Vertex, Record(
                "!!ARBvp1.0\nMOV result.position, vertex.position;\n"
                "MOV result.color, vertex.attrib[1];\nEND\n"));
            typeVertex.Bind();
            auto positions = buffers.Create({GxPoolTarget_Vertex, GxPoolUsage_Static, 12, 3});
            const float points[][3] = {{-0.75f, -0.75f, 0}, {0.75f, -0.75f, 0}, {0, 0.75f, 0}};
            buffers.Upload(positions, points, sizeof(points));
            const uint8_t packed[][4] = {{64, 128, 192, 255}, {64, 128, 192, 255}, {64, 128, 192, 255}};
            const float floats[][3] = {{0.25f, 0.5f, 0.75f}, {0.25f, 0.5f, 0.75f}, {0.25f, 0.5f, 0.75f}};
            const int16_t shorts[][2] = {{8192, 16384}, {8192, 16384}, {8192, 16384}};
            for (uint32_t type = 0; type < 7; ++type) {
                uint32_t size = type == 3 ? 8 : type == 4 ? 12 : 4;
                auto values = buffers.Create({GxPoolTarget_Vertex, GxPoolUsage_Static, size, 3});
                auto write = buffers.Lock(values);
                for (uint32_t i = 0; i < 3; ++i) {
                    const void* source = type <= 2 ? static_cast<const void*>(packed[i]) :
                        type == 5 ? static_cast<const void*>(shorts[i]) : static_cast<const void*>(floats[i]);
                    std::memcpy(static_cast<uint8_t*>(write.Data()) + i * size, source, size);
                }
                write.Commit();
                const GlVertexBinding format[] = {{positions, 0, 4, 0, 0}, {values, 1, type, 0, 0}};
                buffers.VertexFormat(format, 2);
                Clear(GL_COLOR_BUFFER_BIT);
                buffers.Draw(GxPrim_Triangles, 0, 3);
                if (type <= 2) Pixel(type == 1 ? 255 : 64, type == 1 ? 255 : 128, type == 1 ? 255 : 192);
                else Pixel(64, type == 6 ? 0 : 128, type == 4 ? 191 : 0);
                bindings();
            }
            Disable(GL_VERTEX_PROGRAM_ARB);
            Reject<std::logic_error>([&] { buffers.Draw(GxPrim_Triangles, 0, 3); });
            Enable(GL_VERTEX_PROGRAM_ARB);
            buffers.VertexFormat(nullptr, 0); buffers.IndexBuffer();
            Reject<std::logic_error>([&] { buffers.Draw(GxPrim_Points, 0, 1); });
            bindings();
        }
        bindings();
        BindVertexArray(0); BindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
        DeleteVertexArrays(1, &foreignVao); DeleteBuffersARB(2, foreignBuffers);
        targets.Bind();
        Disable(GL_VERTEX_PROGRAM_ARB); Disable(GL_FRAGMENT_PROGRAM_ARB);
        Clean();
        std::cout << "gx-buffer-upload-lock-abort-retention-bounds-affinity=passed\n"
                     "gx-seven-attribute-types-separate-streams-zero-stride=passed\n"
                     "gx-buffered-array-indexed-pct-pixels-binding-restoration=passed\n";
    }
};
}
int main() {
    try {
        StormRtlInitialize(); OsTimeStartup(OsTimeManager::BestAvailable);
        for (int cycle = 0; cycle < 3; ++cycle) {
            GlContext context("Gx buffer fixtures", 96, 64, true);
            std::cout << "gpu=" << context.Info().vendor << ";" << context.Info().renderer << ";"
                      << context.Info().version << ";video=" << context.Info().video << '\n';
            Checks(context).Run();
            std::cout << "gx-buffer-cycle=" << cycle + 1 << ";passed\n";
        }
        OsTimeShutdown(); StormRtlDestroy();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Native buffer checks failed: " << error.what() << '\n';
        return 1;
    }
}
