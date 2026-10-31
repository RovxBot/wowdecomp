// Authored native ABI/WSI fixture for #133; no original-client or asset claim.
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <d3d9.h>
#include <wsi/native_sdl3.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {
void Check(HRESULT result, const char* operation) {
    if (FAILED(result)) {
        char message[160];
        std::snprintf(message, sizeof(message), "%s failed: HRESULT=0x%08x", operation, unsigned(result));
        throw std::runtime_error(message);
    }
}
void Sdl(bool result, const char* operation) {
    if (!result) throw std::runtime_error(std::string(operation) + ": " + SDL_GetError());
}
template<class T> struct Com {
    T* ptr = nullptr;
    Com() = default;
    Com(const Com&) = delete;
    Com& operator=(const Com&) = delete;
    ~Com() { if (ptr) ptr->Release(); }
    T* operator->() const { return ptr; }
    T** Out() { if (ptr) throw std::logic_error("Release COM resource before replacement"); return &ptr; }
};
struct Video {
    Video() { SDL_SetMainReady(); Sdl(SDL_Init(SDL_INIT_VIDEO), "SDL_Init"); }
    ~Video() { SDL_Quit(); }
};
struct Window {
    SDL_Window* ptr;
    explicit Window(bool visible) : ptr(SDL_CreateWindow("wowdecomp DXVK Native prototype", 320, 240,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | (visible ? 0 : SDL_WINDOW_HIDDEN))) {
        Sdl(ptr != nullptr, "SDL_CreateWindow");
    }
    ~Window() { SDL_DestroyWindow(ptr); }
};
struct Vertex { float x, y, z, rhw; uint32_t color; float u0, v0, u1, v1; };
constexpr DWORD vertexFormat = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX2;
void Quad(IDirect3DDevice9* device, float depth, uint32_t color, float width, float height) {
    const Vertex vertices[] = {
        {0, 0, depth, 1, color, 0, 0, 0, 0}, {width, 0, depth, 1, color, 1, 0, 1, 0},
        {0, height, depth, 1, color, 0, 1, 0, 1}, {width, height, depth, 1, color, 1, 1, 1, 1}
    };
    Check(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(Vertex)), "DrawPrimitiveUP");
}
void SolidTexture(IDirect3DDevice9* device, Com<IDirect3DTexture9>& texture, uint32_t color) {
    Check(device->CreateTexture(2, 2, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, texture.Out(), nullptr), "CreateTexture");
    D3DLOCKED_RECT pixels{};
    Check(texture->LockRect(0, &pixels, nullptr, 0), "Texture LockRect");
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 2; ++x)
            std::memcpy(static_cast<char*>(pixels.pBits) + y * pixels.Pitch + x * 4, &color, 4);
    Check(texture->UnlockRect(0), "Texture UnlockRect");
}
void Readback(IDirect3DDevice9* device, IDirect3DSurface9* surface, uint32_t expected,
              const std::string& snapshot) {
    D3DSURFACE_DESC desc{};
    Check(surface->GetDesc(&desc), "GetDesc");
    Com<IDirect3DSurface9> copy;
    Check(device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format,
        D3DPOOL_SYSTEMMEM, copy.Out(), nullptr), "CreateReadback");
    Check(device->GetRenderTargetData(surface, copy.ptr), "GetRenderTargetData");
    D3DLOCKED_RECT pixels{};
    Check(copy->LockRect(&pixels, nullptr, D3DLOCK_READONLY), "Readback LockRect");
    struct Unlock {
        IDirect3DSurface9* surface;
        ~Unlock() { surface->UnlockRect(); }
    } unlock{copy.ptr};
    const auto pixel = [&](UINT x, UINT y) {
        uint32_t value;
        std::memcpy(&value, static_cast<const char*>(pixels.pBits) + y * pixels.Pitch + x * 4, 4);
        return value & 0xffffff;
    };
    // Interior points avoid raster-edge and format-alpha ambiguity.
    for (auto point : std::array<std::array<UINT, 2>, 3>{{
        {{desc.Width / 4, desc.Height / 4}}, {{desc.Width / 2, desc.Height / 2}},
        {{desc.Width * 3 / 4, desc.Height * 3 / 4}}}}) {
        const auto actual = pixel(point[0], point[1]);
        std::printf("pixel=%ux%u actual=%06x expected=%06x\n", point[0], point[1], actual, expected);
        if (actual != expected) throw std::runtime_error("D3D9 pixel mismatch");
    }
    if (!snapshot.empty()) {
        std::ofstream output(snapshot, std::ios::binary);
        output << "P6\n" << desc.Width << ' ' << desc.Height << "\n255\n";
        for (UINT y = 0; y < desc.Height; ++y)
            for (UINT x = 0; x < desc.Width; ++x) {
                auto value = pixel(x, y);
                const char rgb[] = {char(value >> 16), char(value >> 8), char(value)};
                output.write(rgb, 3);
            }
        if (!output) throw std::runtime_error("Snapshot write failed");
    }
}
void Scene(IDirect3DDevice9* device, const std::string& snapshot) {
    Com<IDirect3DSurface9> back, oldDepth, target, depth;
    Com<IDirect3DTexture9> offscreen, first, second;
    Check(device->GetRenderTarget(0, back.Out()), "GetRenderTarget");
    Check(device->GetDepthStencilSurface(oldDepth.Out()), "GetDepthStencilSurface");
    D3DSURFACE_DESC backDesc{};
    Check(back->GetDesc(&backDesc), "Backbuffer GetDesc");
    Check(device->CreateTexture(64, 64, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
        D3DPOOL_DEFAULT, offscreen.Out(), nullptr), "CreateRenderTexture");
    Check(offscreen->GetSurfaceLevel(0, target.Out()), "GetSurfaceLevel");
    Check(device->CreateDepthStencilSurface(64, 64, D3DFMT_D24S8, D3DMULTISAMPLE_NONE,
        0, TRUE, depth.Out(), nullptr), "CreateDepthStencilSurface");
    SolidTexture(device, first, 0xffff8000);
    SolidTexture(device, second, 0xff80ff00);
    Check(device->SetRenderTarget(0, target.ptr), "SetRenderTarget");
    Check(device->SetDepthStencilSurface(depth.ptr), "SetDepthStencilSurface");
    Check(device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff204080, 1, 0), "Clear offscreen");
    Check(device->SetFVF(vertexFormat), "SetFVF");
    Check(device->SetRenderState(D3DRS_LIGHTING, FALSE), "SetLighting");
    Check(device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE), "SetCull");
    Check(device->SetRenderState(D3DRS_ZENABLE, TRUE), "SetDepth");
    Check(device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL), "SetDepthFunction");
    Check(device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE), "SetDepthWrite");
    for (DWORD stage = 0; stage < 2; ++stage) {
        Check(device->SetTexture(stage, stage ? second.ptr : first.ptr), "SetTexture");
        Check(device->SetTextureStageState(stage, D3DTSS_TEXCOORDINDEX, stage), "SetTexcoord");
        Check(device->SetTextureStageState(stage, D3DTSS_COLOROP, D3DTOP_MODULATE), "SetColorOp");
        Check(device->SetTextureStageState(stage, D3DTSS_COLORARG1, D3DTA_TEXTURE), "SetColorArg1");
        Check(device->SetTextureStageState(stage, D3DTSS_COLORARG2, stage ? D3DTA_CURRENT : D3DTA_DIFFUSE), "SetColorArg2");
        Check(device->SetTextureStageState(stage, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1), "SetAlphaOp");
        Check(device->SetTextureStageState(stage, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE), "SetAlphaArg");
        Check(device->SetSamplerState(stage, D3DSAMP_MINFILTER, D3DTEXF_POINT), "SetMinFilter");
        Check(device->SetSamplerState(stage, D3DSAMP_MAGFILTER, D3DTEXF_POINT), "SetMagFilter");
    }
    Check(device->SetTextureStageState(2, D3DTSS_COLOROP, D3DTOP_DISABLE), "Disable third stage");
    Check(device->BeginScene(), "BeginScene");
    Quad(device, 0.25f, 0xffffffff, 64, 64); // two distinct textures -> 0x808000
    Quad(device, 0.75f, 0xff000000, 64, 64); // farther black must fail depth
    Check(device->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE), "Enable alpha test");
    Check(device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER), "SetAlphaFunction");
    Check(device->SetRenderState(D3DRS_ALPHAREF, 127), "SetAlphaReference");
    Quad(device, 0.1f, 0x00000000, 64, 64); // nearer transparent must fail alpha
    Check(device->EndScene(), "EndScene");
    Readback(device, target.ptr, 0x808000, "");
    Check(device->SetTexture(0, nullptr), "Unbind first input");
    Check(device->SetTexture(1, nullptr), "Unbind second input");
    Check(device->SetRenderTarget(0, back.ptr), "Restore backbuffer");
    Check(device->SetDepthStencilSurface(oldDepth.ptr), "Restore depth");
    Check(device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE), "Disable alpha test");
    Check(device->SetRenderState(D3DRS_ZENABLE, FALSE), "Disable depth");
    Check(device->SetTexture(0, offscreen.ptr), "Sample completed offscreen");
    Check(device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1), "Select offscreen color");
    Check(device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE), "Disable second stage");
    Check(device->BeginScene(), "Begin presentation scene");
    Quad(device, 0, 0xffffffff, float(backDesc.Width), float(backDesc.Height));
    Check(device->EndScene(), "End presentation scene");
    Readback(device, back.ptr, 0x808000, snapshot);
    Check(device->SetTexture(0, nullptr), "Unbind completed offscreen");
    Check(device->Present(nullptr, nullptr, nullptr, nullptr), "Present");
}
void Cycle(bool visible, const std::string& snapshot, unsigned cycle) {
    Video video;
    Window window(visible);
    Com<IDirect3D9> d3d;
    d3d.ptr = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d.ptr) throw std::runtime_error("Direct3DCreate9 returned null");
    D3DADAPTER_IDENTIFIER9 adapter{};
    Check(d3d->GetAdapterIdentifier(0, 0, &adapter), "GetAdapterIdentifier");
    std::printf("cycle=%u video=%s adapter=%s vendor=%04x device=%04x driver=%s\n",
        cycle, SDL_GetCurrentVideoDriver(), adapter.Description, adapter.VendorId, adapter.DeviceId, adapter.Driver);
    D3DPRESENT_PARAMETERS params{};
    params.BackBufferWidth = 320;
    params.BackBufferHeight = 240;
    params.BackBufferFormat = D3DFMT_X8R8G8B8;
    params.BackBufferCount = 1;
    params.SwapEffect = D3DSWAPEFFECT_DISCARD;
    params.hDeviceWindow = dxvk::wsi::toHwnd(window.ptr);
    params.Windowed = TRUE;
    params.EnableAutoDepthStencil = TRUE;
    params.AutoDepthStencilFormat = D3DFMT_D24S8;
    params.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    Com<IDirect3DDevice9> device;
    Check(d3d->CreateDevice(0, D3DDEVTYPE_HAL, params.hDeviceWindow,
        D3DCREATE_HARDWARE_VERTEXPROCESSING, &params, device.Out()), "CreateDevice");
    D3DCAPS9 caps{};
    Check(device->GetDeviceCaps(&caps), "GetDeviceCaps");
    static_assert(sizeof(DWORD) == 4 && sizeof(HRESULT) == 4 && sizeof(void*) == 8, "Native D3D9 ABI mismatch");
    std::printf("caps vertexConstants=%u simultaneousTextures=%u vertexShader=%08x pixelShader=%08x\n",
        caps.MaxVertexShaderConst, caps.MaxSimultaneousTextures, caps.VertexShaderVersion, caps.PixelShaderVersion);
    if (caps.MaxSimultaneousTextures < 2) throw std::runtime_error("Two texture stages unavailable");
    Scene(device.ptr, "");
    // Scene releases every external default-pool reference before Reset.
    Sdl(SDL_SetWindowSize(window.ptr, 400, 300), "SDL_SetWindowSize");
    Sdl(SDL_SyncWindow(window.ptr), "SDL_SyncWindow");
    SDL_PumpEvents();
    int width = 0, height = 0;
    Sdl(SDL_GetWindowSizeInPixels(window.ptr, &width, &height), "SDL_GetWindowSizeInPixels");
    if (width <= 0 || height <= 0) throw std::runtime_error("Invalid resized drawable extent");
    params.BackBufferWidth = UINT(width);
    params.BackBufferHeight = UINT(height);
    Check(device->Reset(&params), "Reset after resize");
    const auto started = std::chrono::steady_clock::now();
    Scene(device.ptr, snapshot);
    const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    std::printf("cycle=%u resize=%dx%d sceneWithReadbackMs=%.3f passed\n", cycle, width, height, elapsed);
    if (visible) {
        const auto end = SDL_GetTicks() + 1500;
        while (SDL_GetTicks() < end) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) if (event.type == SDL_EVENT_QUIT) return;
            SDL_Delay(10);
        }
    }
}
}
int main(int argc, char** argv) {
    bool visible = false;
    std::string snapshot;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--visible") == 0) visible = true;
        else if (std::strcmp(argv[i], "--snapshot") == 0 && i + 1 < argc) snapshot = argv[++i];
        else { std::fprintf(stderr, "Usage: NativeD3D9Check [--visible] [--snapshot path.ppm]\n"); return 2; }
    }
    try {
        Sdl(SDL_setenv_unsafe("DXVK_WSI_DRIVER", "SDL3", 1) == 0, "Select DXVK SDL3 WSI");
        for (unsigned cycle = 1; cycle <= 3; ++cycle) Cycle(visible, cycle == 3 ? snapshot : "", cycle);
        std::puts("native-d3d9-sdl3-multitexture-offscreen-depth-alpha-present-resize-shutdown=passed cycles=3");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "NativeD3D9Check failed: %s\n", error.what());
        return 1;
    }
}
