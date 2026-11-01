// Focused regression of the device used by WowNativeClient, with authored bytes.
#include "Engine/Source/Gx/CGxDeviceD3d/CGxDeviceD3d.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Os/Portable/Runtime.h"
#include "Engine/Source/Os/Time.h"
#include <SDL3/SDL.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>

using Bytes = std::vector<uint8_t>;
namespace {
void Require(bool yes, const char* message) { if (!yes) throw std::runtime_error(message); }
template<class F> void Reject(F operation) { bool rejected = false; try { operation(); } catch (const std::exception&) { rejected = true; } Require(rejected, "Expected an explicit failure"); }
Bytes Package(bool vertex, unsigned count) {
    // Authored vs_1_1 MOV oPos,v0 and ps_1_1 MOV r0,v0; these are fixture
    // bytecode, never substituted for authenticated original-data shaders.
    const std::vector<uint32_t> code = vertex ? std::vector<uint32_t>{0xfffe0101, 1, 0xc00f0000, 0x90e40000, 0xffff}
                                             : std::vector<uint32_t>{0xffff0101, 1, 0x800f0000, 0x90e40000, 0xffff};
    Bytes bytes;
    auto word = [&](uint32_t value) { for (int i = 0; i < 4; ++i) bytes.push_back(uint8_t(value >> (i * 8))); };
    word(0x47585348); word(0x10003); word(0);
    for (unsigned i = 0; i < count; ++i) {
        word(0); word(0); word(0); word(uint32_t(code.size() * 4));
        for (auto value : code) word(value);
    }
    return bytes;
}
Bytes Shader(const std::string& path) {
    const bool vertex = path.find("Vertex") != std::string::npos;
    Require(path.find(vertex ? "vs_3_0" : "ps_3_0") != std::string::npos, "D3D9 did not select its actual shader profile");
    return Package(vertex, vertex ? 2 : 1);
}
void Pixel(CGxDeviceD3d& device) {
    GxSceneClear(GxBufferBit_Color | GxBufferBit_Depth, CImVector(37, 83, 149, 255));
    CiRect rect{0, 0, 4, 4};
    TSGrowableArray<CImVector> pixels;
    device.DeviceReadPixels(rect, pixels);
    Require(pixels.Count() == 16, "Original D3D9 readback extent");
    for (uint32_t i = 0; i < pixels.Count(); ++i) Require(pixels[i].r == 149 && pixels[i].g == 83 && pixels[i].b == 37, "Original Gx clear/readback channel mismatch");
    GxScenePresent();
}
}
int main() {
    StormRtlInitialize();
    try {
        for (unsigned cycle = 0; cycle < 3; ++cycle) {
            OsNative::Window window("Original Gx DXVK client contracts", 320, 240, true, OsNative::WindowGraphics::Vulkan);
            Reject([&] { CGxDeviceD3d failed(window, [](const std::string&) -> Bytes { throw std::runtime_error("authored provider failure"); }); });
            Require(!g_theGxDevicePtr, "Failed shader setup published a device");
            {
                CGxDeviceD3d device(window, Shader);
                Require(g_theGxDevicePtr == &device && device.DeviceApi() == GxApi_D3d9, "Original D3D9 dispatch not installed");
                Require(device.Caps().m_numTmus > 1 && device.Caps().m_shaderConstants[GxSh_Vertex] > 94, "DXVK inherited frozen GL caps");
                bool offThread = false;
                std::thread thread([&] { try { device.ScenePresent(); } catch (const std::exception&) { offThread = true; } });
                thread.join(); Require(offThread, "Off-owner presentation accepted");
                CGxShader* bad = nullptr;
                auto saved = device.m_nativeShaders;
                device.m_nativeShaders = [](const std::string&) { auto bytes = Package(false, 1); bytes[4] = 4; return bytes; };
                Reject([&] { device.ShaderCreate(&bad, GxSh_Pixel, "Shaders\\Pixel", "BadVersion", 1); });
                Require(!bad, "Rejected package published a shader");
                device.m_nativeShaders = saved;
                Pixel(device);
                Require(SDL_SetWindowSize(window.Handle(), 400, 300) && SDL_SyncWindow(window.Handle()), "SDL resize failed");
                device.DeviceWM(GxWM_Size, 0, 0);
                Pixel(device);
            }
            Require(!g_theGxDevicePtr, "D3D9 shutdown retained dispatch");
        }
        Require(!OsLiveThreadCount(), "D3D9 client contract left workers");
        std::cout << "original-gx-dxvk-client-contracts=passed lifetimes=3\n";
        StormRtlDestroy(); return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; StormRtlDestroy(); return 1; }
}
