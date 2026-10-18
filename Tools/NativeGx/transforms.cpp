#include "Engine/Source/Gx/Portable/GlTransforms.h"
#include "Engine/Source/Gx/Portable/GlProgram.h"
#include "Engine/Source/Os/Time.h"
#include "Engine/Source/Storm/H/storm.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

using namespace Gx::Native;
namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template<class Exception, class F> void Reject(F function) {
    bool rejected = false;
    try { function(); } catch (const Exception&) { rejected = true; }
    Require(rejected, "Invalid transform operation accepted");
}
void Near(float actual, float expected) {
    Require(std::abs(actual - expected) < 0.00001f, "Gx transform numeric mismatch");
}
struct Checks {
    GlContext& context;
#define BASE(name) decltype(&gl##name) name = reinterpret_cast<decltype(name)>(context.Proc("gl" #name))
    BASE(GetError); BASE(GetIntegerv); BASE(GetFloatv); BASE(MatrixMode);
    BASE(ClearColor); BASE(ClearDepth); BASE(Clear); BASE(ReadPixels);
    BASE(Enable); BASE(Disable); BASE(DepthFunc); BASE(Color4f);
    BASE(Begin); BASE(End); BASE(Vertex3f);
#undef BASE
    explicit Checks(GlContext& owner) : context(owner) {}
    void Clean() { Require(GetError() == GL_NO_ERROR, "Transform fixture GL error"); }
    void Box(GLenum name, int x, int y, int width, int height) {
        GLint box[4]; GetIntegerv(name, box);
        Require(box[0] == x && box[1] == y && box[2] == width && box[3] == height,
                "Gx normalized pixel rectangle mismatch");
    }
    void Stacks(GlTransforms& transforms) {
        C44Matrix identity;
        for (int i = 0; i < GxXforms_Last; ++i) {
            auto which = static_cast<EGxXform>(i);
            Require(transforms.Get(which) == identity, "Uninitialized Gx stack");
            transforms.Push(which);
            transforms.Scale(which, {2, 3, 4});
            transforms.Translate(which, {1, 2, 3});
            auto transformed = transforms.Get(which);
            Near(transformed.d0, 2); Near(transformed.d1, 6); Near(transformed.d2, 12);
            transforms.Push(which); // level 2
            transforms.Translate(which, {1, 0, 0});
            transforms.Push(which); // level 3
            transforms.Translate(which, {10, 0, 0});
            transforms.Push(which); // saturated; copies level 2 again
            Near(transforms.Get(which).d0, 4);
            transforms.Pop(which); transforms.Pop(which);
            Near(transforms.Get(which).d0, 2);
            transforms.Pop(which); transforms.Pop(which);
            Require(transforms.Get(which) == identity, "Gx stack pop lost its base");
        }
        Reject<std::out_of_range>([&] { transforms.Push(UINT32_MAX); });
        Reject<std::out_of_range>([&] { transforms.Get(GxXforms_Last); });
        auto invalid = identity;
        invalid.d0 = std::numeric_limits<float>::infinity();
        Reject<std::invalid_argument>([&] { transforms.Set(GxXform_World, invalid); });
        Require(transforms.Get(GxXform_World) == identity, "Rejected matrix changed the stack");
        Reject<std::invalid_argument>([&] { transforms.Translate(GxXform_View, {0, NAN, 0}); });
        Require(transforms.Get(GxXform_View) == identity, "Rejected translation changed the stack");
        // GxXform_Projection is a stack slot, distinct from XformSetProjection.
        transforms.Set(GxXform_Projection, C44Matrix(7));
        Require(transforms.Projection() == identity, "Projection stack overwrote device projection");
        std::cout << "gx-scalar-stacks-order-saturation-rejection=passed\n";
    }
    void State(GlRenderTargets& targets, GlTransforms& transforms) {
        auto color = targets.Create({GxTex_2d, 64, 48, GlTargetFormat::Rgba8, true});
        targets.Bind({color});
        transforms.Viewport({{0.125f, 0.875f, 0.25f, 0.75f}, 0.2f, 0.8f});
        transforms.Scissor(true, {0.25f, 0.75f, 0.125f, 0.875f});
        C44Matrix projection;
        projection.a0 = 2; projection.b1 = 3; projection.c2 = 4;
        projection.d0 = 0.25f; projection.d1 = -0.5f; projection.d2 = 0.75f;
        transforms.Projection(projection);
        transforms.Translate(GxXform_World, {0.25f, 0.5f, 0.75f});
        transforms.Scale(GxXform_View, {2, 2, 2});
        Reject<std::logic_error>([&] { transforms.UiConstants(); });
        MatrixMode(GL_TEXTURE);
        transforms.Sync();
        GLint mode = 0; GetIntegerv(GL_MATRIX_MODE, &mode);
        Require(mode == GL_TEXTURE, "Transform sync changed caller matrix mode");
        Box(GL_VIEWPORT, 8, 12, 48, 24); Box(GL_SCISSOR_BOX, 16, 6, 32, 36);
        float matrix[16]; GetFloatv(GL_MODELVIEW_MATRIX, matrix);
        Near(matrix[12], 0.5f); Near(matrix[13], 1); Near(matrix[14], -1.5f);
        GetFloatv(GL_PROJECTION_MATRIX, matrix);
        Near(matrix[0], 2); Near(matrix[5], -3); Near(matrix[10], -4);
        Near(matrix[12], 0.25f); Near(matrix[13], 0.5f); Near(matrix[14], 0.75f);
        auto constants = transforms.UiConstants();
        Near(constants[0][0], 4); Near(constants[0][3], 0.25f);
        Near(constants[1][1], -6); Near(constants[1][3], 0.5f);
        Near(constants[2][2], 8); Near(constants[2][3], 0.75f);
        // The UI contract excludes the world stack.
        transforms.Set(GxXform_World, C44Matrix(9)); transforms.Sync();
        Require(transforms.UiConstants() == constants, "UI constants unexpectedly include world");
        auto before = transforms.Viewport();
        Reject<std::invalid_argument>([&] { transforms.Viewport({{0, 0, 0, 1}, 0, 1}); });
        Reject<std::invalid_argument>([&] { transforms.Viewport({{}, 0.8f, 0.2f}); });
        Reject<std::invalid_argument>([&] { transforms.Viewport({{NAN, 1, 0, 1}, 0, 1}); });
        Near(transforms.Viewport().rect.minX, before.rect.minX);
        Reject<std::invalid_argument>([&] { transforms.Scissor(true, {1, 0, 0, 1}); });
        transforms.Scissor(true, {2, 3, 0, 1}); transforms.Sync();
        Box(GL_SCISSOR_BOX, 64, 0, 0, 48);
        // Preserve the source's literal perspective compression branch. This
        // is a contract check, not a visual-parity fix for Windows OpenGL.
        projection.d3 = 0; projection.c3 = 1;
        transforms.Projection(projection);
        transforms.MasterEnables(1u << GxMasterEnable_NormalProjection);
        transforms.Sync(); GetFloatv(GL_PROJECTION_MATRIX, matrix);
        Near(matrix[0], 0.4f); Near(matrix[5], -0.6f); Near(matrix[11], -1);
        transforms.MasterEnables(0x80000000u);
        transforms.Sync(); GetFloatv(GL_PROJECTION_MATRIX, matrix);
        Near(matrix[0], 2); Near(matrix[5], -3);
        transforms.Set(GxXform_World, C44Matrix()); transforms.Set(GxXform_View, C44Matrix());
        transforms.Projection(C44Matrix()); transforms.MasterEnables(0);
        transforms.Viewport({}); transforms.Scissor(false);
        targets.Bind();
        Reject<std::logic_error>([&] { transforms.UiConstants(); });
        transforms.Sync(); GetFloatv(GL_PROJECTION_MATRIX, matrix);
        Near(matrix[5], 1); Near(matrix[10], -1);
        GLint cull = 0; GetIntegerv(GL_CULL_FACE_MODE, &cull);
        Require(cull == GL_BACK, "Drawable did not restore cull direction");
        Clean();
        std::cout << "gx-target-viewport-scissor-projection-ui-constants=passed\n";
    }
    void Quad() {
        Begin(GL_QUADS);
        Vertex3f(-0.75f, -0.75f, 0); Vertex3f(-0.25f, -0.75f, 0);
        Vertex3f(-0.25f, -0.25f, 0); Vertex3f(-0.75f, -0.25f, 0);
        End();
    }
    void Color(int x, int y, bool green) {
        std::array<uint8_t,4> pixel{};
        ReadPixels(x,y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data());
        Require(pixel[0] == 0 && pixel[1] == (green ? 255 : 0) && pixel[2] == 0,
                "Gx transformed/scissored color pixel mismatch");
    }
    void Depth(int x, int y, float expected) {
        float value = 0;
        ReadPixels(x,y,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&value);
        Near(value, expected);
    }
    void Raster(GlRenderTargets& targets, GlTransforms& transforms) {
        auto color = targets.Create({GxTex_2d, 64, 48, GlTargetFormat::Rgba8, true});
        targets.Bind({color});
        Disable(GL_VERTEX_PROGRAM_ARB); Disable(GL_FRAGMENT_PROGRAM_ARB);
        Disable(GL_BLEND); Disable(GL_DITHER); Disable(GL_MULTISAMPLE);
        Disable(GL_TEXTURE_2D); Enable(GL_DEPTH_TEST); Enable(GL_CULL_FACE);
        DepthFunc(GL_LESS);
        ClearColor(0,0,0,1); ClearDepth(1); Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        transforms.Translate(GxXform_World, {0.5f,0,0.4f});
        transforms.Translate(GxXform_View, {0,0.25f,0});
        transforms.Viewport({{},0.2f,0.8f});
        transforms.Scissor(true, {0.5f,0.75f,0.5f,1});
        transforms.Sync();
        Color4f(0,1,0,1); Quad();
        Color(33,30,true); Color(31,30,false); Color(33,18,false);
        Depth(33,30,0.62f); Depth(31,30,1);
        // Exercise the four env constants with an ARB shader. Original .bls
        // programs are tested separately by NativeGlCheck using external data.
        auto record = [](const char* code) {
            ShaderRecord shader; shader.code.assign(code, code + std::strlen(code) + 1); return shader;
        };
        GlProgram vertex(context, ShaderStage::Vertex, record(
            "!!ARBvp1.0\nDP4 result.position.x, program.env[0], vertex.position;\n"
            "DP4 result.position.y, program.env[1], vertex.position;\n"
            "DP4 result.position.z, program.env[2], vertex.position;\n"
            "DP4 result.position.w, program.env[3], vertex.position;\n"
            "MOV result.color, vertex.color;\nEND\n"));
        GlProgram fragment(context, ShaderStage::Fragment, record(
            "!!ARBfp1.0\nMOV result.color, fragment.color;\nEND\n"));
        transforms.Set(GxXform_World, C44Matrix(20)); // UI must exclude world.
        C44Matrix view; view.Translate({0.5f,0.25f,0.4f});
        transforms.Set(GxXform_View, view); transforms.Scissor(false); transforms.Sync();
        Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        auto constants = transforms.UiConstants();
        for (uint32_t i = 0; i < 4; ++i) {
            float value[4]; std::copy(constants[i].begin(), constants[i].end(), value);
            vertex.Constant(i, value);
        }
        vertex.Bind(); fragment.Bind(); Enable(GL_VERTEX_PROGRAM_ARB); Enable(GL_FRAGMENT_PROGRAM_ARB);
        Quad(); Color(33,30,true); Color(16,12,false); Depth(33,30,0.62f);
        Disable(GL_VERTEX_PROGRAM_ARB); Disable(GL_FRAGMENT_PROGRAM_ARB);
        // Depth-only targets still need offscreen projection/culling and the
        // actual attachment extent, without a color attachment to infer it.
        auto depth = targets.Create({GxTex_2d,64,48,GlTargetFormat::Depth24,false});
        targets.Bind({}, {depth});
        transforms.Set(GxXform_World, C44Matrix()); transforms.Sync();
        Clear(GL_DEPTH_BUFFER_BIT); Quad(); Depth(33,30,0.62f);
        targets.Bind(); transforms.Set(GxXform_View, C44Matrix());
        transforms.Viewport({}); transforms.Sync();
        Disable(GL_CULL_FACE); Clean();
        std::cout << "gx-fixed-and-arb-transformed-depth-scissor-cull-pixels=passed\n";
    }
    void Resize(GlRenderTargets& targets, GlTransforms& transforms) {
        Require(SDL_SetWindowSize(context.Window().Handle(),517,293), "Cannot resize transform fixture");
        int w=0,h=0,pw=0,ph=0;
        for (int i=0;i<100;++i) {
            context.Window().Pump(); context.Window().Size(w,h,pw,ph);
            if (w==517 && h==293) break;
            SDL_Delay(10);
        }
        Require(w==517 && h==293 && pw>0 && ph>0, "Transform fixture resize not observed");
        targets.Bind();
        transforms.Viewport({{0.25f,0.75f,0.25f,0.75f},0,1}); transforms.Sync();
        auto rounded = [](int size, float fraction) { return int(size * fraction + 0.5f); };
        Box(GL_VIEWPORT,rounded(pw,0.25f),rounded(ph,0.25f),
            rounded(pw,0.75f)-rounded(pw,0.25f),rounded(ph,0.75f)-rounded(ph,0.25f));
        Clean();
        std::cout << "gx-resized-drawable-normalized-pixel-viewport=passed logical=" << w << ',' << h
                  << " pixels=" << pw << ',' << ph << '\n';
    }
};
}
int main() {
    StormRtlInitialize(); OsTimeStartup(OsTimeManager::BestAvailable);
    int result=0;
    try {
        for (int cycle=0;cycle<3;++cycle) {
            GlContext context("M3 Gx transform fixtures",320,240,true);
            std::cout << "gl-vendor=" << context.Info().vendor << "\ngl-renderer=" << context.Info().renderer
                      << "\ngl-version=" << context.Info().version << '\n';
            GlRenderTargets targets(context); targets.Bind();
            GlTransforms transforms(context,targets);
            Checks checks(context);
            // Sync before the fixture makes any matrix-mode selection. The
            // adapter must initialize the compatibility baseline itself.
            transforms.Sync();
            GLint initialMode = 0; checks.GetIntegerv(GL_MATRIX_MODE, &initialMode);
            Require(initialMode == GL_MODELVIEW, "Missing initial Gx matrix-mode baseline");
            checks.Stacks(transforms); checks.State(targets,transforms);
            checks.Raster(targets,transforms); checks.Resize(targets,transforms);
            std::thread wrong([&] {
                Reject<std::logic_error>([&] { transforms.Sync(); });
                Reject<std::logic_error>([&] { transforms.Set(GxXform_World,C44Matrix()); });
                Reject<std::logic_error>([&] { transforms.Get(GxXform_View); });
                Reject<std::logic_error>([&] { transforms.UiConstants(); });
            });
            wrong.join();
        }
        std::cout << "native-transform-cycles=3 passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; result=1;
    }
    OsTimeShutdown(); StormRtlDestroy(); return result;
}
