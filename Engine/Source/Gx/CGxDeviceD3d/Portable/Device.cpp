// Native platform/resource ownership for the original 12340 D3D9 Gx renderer.
// No Windows address/layout or verification claim applies to this adaptation.
#include "Engine/Source/Gx/CGxDeviceD3d/CGxDeviceD3d.h"
#include "Engine/Source/Gx/CGxPool.h"
#include "Engine/Source/Gx/Portable/ShaderPackage.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Storm/H/storm.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace {
void Require(bool yes, const char* message) { if (!yes) throw std::runtime_error(message); }
void Result(HRESULT result, const char* operation) {
    if (FAILED(result)) throw std::runtime_error(std::string(operation) + " HRESULT=" + std::to_string(uint32_t(result)));
}
template<class T> void Release(T*& object) { if (object) object->Release(); object = nullptr; }
void Unavailable(const char* operation) { throw std::logic_error(operation); }
}

CGxDeviceD3d::CGxDeviceD3d(OsNative::Window& window, ShaderBytes shaders)
    : m_hwnd(reinterpret_cast<HWND>(window.Handle())), m_hwndClass(0), m_ownhwnd(0),
      m_d3dLib(nullptr), m_d3d(nullptr), m_d3dDevice(nullptr), m_d3dCaps{},
      m_d3dIsHwDevice(0), m_d3dNVAPI(0), m_d3dStereoEnabled(0), m_d3dStereoRestore(0),
      m_d3dStereoHandle(nullptr), m_d3dStereoConvergence(1), m_d3dStereoSeparation(25),
      m_d3dStereoDirty(0), m_d3dVertexDecl{}, m_desktopDisplayMode{}, m_inScene(0),
      m_ambientOnlyMode(0), m_devDepthFormat(D3DFMT_UNKNOWN), m_devAdapterFormat(D3DFMT_UNKNOWN),
      m_defDepthStencilSurface(nullptr), m_defColorSurface(nullptr), m_defDepthSurface(nullptr),
      m_surface3B44(nullptr), m_eventQuery(nullptr), m_hwCursorDirty(0), m_hwCursorTexture(nullptr),
      m_hwCursorBitmap(nullptr), m_texture3B58(nullptr), m_d3dCurrentVertexDecl(nullptr),
      m_d3dCurrentIndexBuf(nullptr), m_d3dVertexStreamBuf{}, m_d3dVertexStreamOfs{},
      m_d3dVertexStreamStride{}, m_texEnabled{}, m_nativeWindow(&window), m_nativeShaders(std::move(shaders)) {
    NativeCheckOwner();
    Require(!g_theGxDevicePtr, "A Gx device is already installed");
    m_api = GxApi_D3d9;
    m_caps.m_colorFormat = GxCF_argb;
    m_caps.unk_0018 = 1;
    std::memset(m_deviceStates, 0xff, sizeof(m_deviceStates));
    try {
        m_d3d = Direct3DCreate9(D3D_SDK_VERSION);
        Require(m_d3d, "DXVK Native Direct3DCreate9 failed");
        Result(m_d3d->GetDeviceCaps(0, D3DDEVTYPE_HAL, &m_d3dCaps), "GetDeviceCaps");
        Result(m_d3d->GetAdapterDisplayMode(0, &m_desktopDisplayMode), "GetAdapterDisplayMode");
        int width, height, pixelWidth, pixelHeight;
        window.Size(width, height, pixelWidth, pixelHeight);
        Require(pixelWidth > 0 && pixelHeight > 0, "D3D9 requires a nonempty SDL drawable");
        m_defWindowRect = m_curWindowRect = CRect{0, 0, float(pixelHeight), float(pixelWidth)};
        m_format = CGxFormat(true, {int32_t(m_defWindowRect.maxX), int32_t(m_defWindowRect.maxY)},
                            CGxFormat::Fmt_Argb8888, CGxFormat::Fmt_Ds248, 0, 0, true, false, false, true, 0);
        D3DPRESENT_PARAMETERS present;
        ISetPresentParms(present, m_format);
        // SDL owns the window, focus, events and fullscreen state. DXVK's native
        // HWND is that same SDL_Window*, never an X11/Wayland or Win32 handle.
        Result(m_d3d->CreateDevice(0, D3DDEVTYPE_HAL, m_hwnd,
            D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &present, &m_d3dDevice), "CreateDevice");
        m_d3dIsHwDevice = 1;
        m_context = 1;
        m_devAdapterFormat = present.BackBufferFormat;
        m_devDepthFormat = present.AutoDepthStencilFormat;
        ISetCaps(m_format);
        // Original shadow storage holds 256 float4s. Report this backend's actual
        // shader budgets; frozen GL's 94-register/one-sampler limits do not apply.
        Require(m_caps.m_shaderConstants[GxSh_Vertex] <= 256, "Vertex constants exceed original shadow storage");
        m_caps.m_shaderConstants[GxSh_Pixel] = m_caps.m_shaderTargets[GxSh_Pixel] == 4 ? 224 : 32;
        m_caps.m_hwCursor = m_caps.m_stereoAvailable = 0;
        DeviceCreatePools();
        DeviceCreateStreamBufs();
        IStateSetD3DDefaults();
        m_windowVisible = m_windowFocus = 1;
        // Creation uses the caller's authenticated 0x10003 package provider.
        ShaderCreate(s_uiVertexShader, GxSh_Vertex, "Shaders\\Vertex", "UI", 2);
        ShaderCreate(&s_uiPixelShader, GxSh_Pixel, "Shaders\\Pixel", "UI", 1);
        g_theGxDevicePtr = this; // Publish only the completed device.
    } catch (...) { DeviceDestroy(); throw; }
}
CGxDeviceD3d::~CGxDeviceD3d() { DeviceDestroy(); }
void CGxDeviceD3d::NativeCheckOwner() const {
    Require(m_nativeWindow && m_nativeWindow->Handle() == reinterpret_cast<SDL_Window*>(m_hwnd),
            "D3D9 requires the owning SDL main-thread window");
}
void CGxDeviceD3d::NativeSyncTransforms() { NativeCheckOwner(); IStateSyncXforms(); }
void CGxDeviceD3d::NativeBufDestroy(CGxBuf* buf) {
    NativeCheckOwner();
    Require(buf && buf != m_bufLocked[0] && buf != m_bufLocked[1], "Destroying a locked D3D9 buffer");
    if (m_primVertexBuf == buf) m_primVertexBuf = nullptr;
    if (m_primIndexBuf == buf) { m_primIndexBuf = nullptr; m_primIndexDirty = 1; }
    for (auto& binding : m_primVertexFormatBuf) if (binding == buf) binding = nullptr;
    for (auto& stream : m_streamBufs) if (stream == buf) stream = nullptr;
    m_primVertexDirty = 0xffffffff;
}
void CGxDeviceD3d::AbortBufferWrite(CGxBuf* buf) {
    NativeCheckOwner();
    Require(buf && buf->m_pool && m_bufLocked[buf->m_pool->m_target] == buf, "D3D9 buffer is not locked");
    IBufUnlock(buf);
    CGxDevice::BufUnlock(buf, 0);
    buf->m_hasData = 0;
}
void CGxDeviceD3d::DeviceDestroy() {
    NativeCheckOwner();
    ISceneEnd();
    for (auto& target : m_textureTarget) { Release(reinterpret_cast<IDirect3DSurface9*&>(target.m_apiSpecific)); target.m_texture = nullptr; }
    while (auto* query = m_queryList.Head()) QueryDestroy(query);
    while (auto* texture = m_textures.Head()) TexDestroy(texture);
    s_uiVertexShader[0] = s_uiVertexShader[1] = s_uiPixelShader = nullptr;
    IReleaseD3dShaders(1);
    for (auto& list : m_shaderList) list.Clear();
    while (auto* pool = m_poolList.Head()) PoolDestroy(pool);
    m_vertexPool = m_indexPool = nullptr;
    m_streamBufs[0] = m_streamBufs[1] = nullptr;
    IReleaseD3dVertexDecl();
    Release(m_defColorSurface); Release(m_defDepthSurface); Release(m_defDepthStencilSurface);
    Release(m_surface3B44); Release(m_eventQuery);
    Release(m_d3dDevice); Release(m_d3d);
    m_context = 0;
    if (g_theGxDevicePtr == this) g_theGxDevicePtr = nullptr;
}
void CGxDeviceD3d::ScenePresent() {
    NativeCheckOwner();
    Require(m_context && m_d3dDevice, "D3D9 context unavailable; device-loss recovery is unqualified");
    ISceneEnd();
    if (m_scrShotClick) DeviceScreenShot();
    Result(m_d3dDevice->Present(nullptr, nullptr, nullptr, nullptr), "DXVK Present");
    CGxDevice::ScenePresent();
    ISceneBegin();
    Require(m_inScene, "DXVK BeginScene failed");
}
void CGxDeviceD3d::ISetWindowFocus(int32_t focus) { NativeCheckOwner(); m_windowFocus = focus; }
int32_t CGxDeviceD3d::DeviceWM(EGxWM wm, uintptr_t, uintptr_t) {
    NativeCheckOwner();
    if (wm == GxWM_Size) {
        int width, height, pixelWidth, pixelHeight;
        m_nativeWindow->Size(width, height, pixelWidth, pixelHeight);
        if (pixelWidth <= 0 || pixelHeight <= 0) return 0;
        Require(!m_bufLocked[0] && !m_bufLocked[1] && !m_textureTarget[0].m_texture && !m_textureTarget[1].m_texture,
                "D3D9 resize requires retired buffer writes/offscreen targets");
        ISceneEnd();
        IReleaseD3dResources(0);
        m_defWindowRect = m_curWindowRect = CRect{0, 0, float(pixelHeight), float(pixelWidth)};
        D3DPRESENT_PARAMETERS present;
        ISetPresentParms(present, m_format);
        m_context = 0;
        Result(m_d3dDevice->Reset(&present), "DXVK resize Reset");
        m_context = 1;
        IStateSetD3DDefaults();
        m_viewportDirty = 1;
        NotifyOnDeviceRestored();
    } else if (wm == GxWM_KillFocus || wm == GxWM_Destroy) m_windowFocus = 0;
    else if (wm == GxWM_SetFocus) m_windowFocus = 1;
    return 0;
}
void CGxDeviceD3d::ITexForceRecreation(int32_t all) {
    // Release only resources that Reset requires; never put a CGxTex* into a COM slot.
    for (auto* texture = m_textures.Head(); texture; texture = m_textures.Next(texture)) {
        if (!(all || texture->m_flags.m_renderTarget)) continue;
        Release(reinterpret_cast<IDirect3DBaseTexture9*&>(texture->m_apiSpecificData));
        texture->m_needsCreation = 1;
        texture->m_needsUpdate = 1;
    }
}
void CGxDeviceD3d::CapsWindowSizeInScreenCoords(CRect&) { Unavailable("SDL screen-coordinate Gx query unavailable"); }
void CGxDeviceD3d::DeviceSetGamma(float) { Unavailable("DXVK native display gamma unavailable"); }
void CGxDeviceD3d::DeviceSetGamma(const CGxGammaRamp&) { Unavailable("DXVK native display gamma unavailable"); }
void CGxDeviceD3d::CursorSetVisible(int32_t visible) {
    NativeCheckOwner();
    Require(visible ? SDL_ShowCursor() : SDL_HideCursor(), "SDL cursor visibility failed");
    CGxDevice::CursorSetVisible(visible);
}
int32_t CGxDeviceD3d::DeviceCreate(GxWindowProc, const CGxFormat&) { Unavailable("SDL window is caller-owned"); return 0; }
int32_t CGxDeviceD3d::DeviceCreate(void*, const CGxFormat&) { Unavailable("SDL window is caller-owned"); return 0; }
int32_t CGxDeviceD3d::DeviceSetFormat(const CGxFormat&) { Unavailable("DXVK format recreation unavailable"); return 0; }
void CGxDeviceD3d::DeviceUnk81(uint32_t, uint32_t) { Unavailable("Unknown D3D9 operation 81"); }
void CGxDeviceD3d::DeviceUnk82() { Unavailable("Unknown D3D9 operation 82"); }
void CGxDeviceD3d::DeviceUnk83(uint32_t, uint32_t) { Unavailable("Unknown D3D9 operation 83"); }

void CGxDeviceD3d::ShaderCreate(CGxShader** out, EGxShTarget target, const char* path, const char* name, uint32_t count) {
    NativeCheckOwner();
    Require(out && path && name && count && count <= 65536 && (target == GxSh_Vertex || target == GxSh_Pixel), "Invalid D3D9 shader request");
    std::fill_n(out, count, nullptr);
    const auto profile = m_caps.m_shaderTargets[target];
    Require(profile > 0, "DXVK shader profile unavailable");
    const char* dialect = target == GxSh_Vertex ? s_vertexShaderTargetNames[profile] : s_pixelShaderTargetNames[profile];
    const std::string member = std::string(path) + "\\" + dialect + "\\" + name + ".bls";
    auto& list = m_shaderList[target];
    std::vector<std::string> keys;
    bool missing = false;
    for (uint32_t i = 0; i < count; ++i) {
        keys.push_back(member + ":" + std::to_string(count) + ":" + std::to_string(i));
        missing |= !list.Ptr(keys.back().c_str());
    }
    Gx::Native::ShaderPackage package;
    if (missing) {
        Require(bool(m_nativeShaders), "Authenticated D3D9 shader provider unavailable");
        package = Gx::Native::DecodeShaderPackage(m_nativeShaders(member), count);
    }
    std::vector<CGxShader*> created;
    created.reserve(count);
    try {
        for (uint32_t i = 0; i < count; ++i) {
            if (list.Ptr(keys[i].c_str())) continue;
            const auto& record = package.permutations[i];
            if (!record.code.empty()) {
                Require(record.code.size() >= 8 && record.code.size() % 4 == 0, "Unaligned/truncated D3D9 bytecode");
                const auto& code = record.code;
                Require(code[code.size()-4] == 0xff && code[code.size()-3] == 0xff && !code[code.size()-2] && !code.back(), "D3D9 bytecode missing END");
            }
            auto* shader = list.New(keys[i].c_str(), 0, 0);
            created.push_back(shader);
            shader->m_target = target;
            shader->m_code.SetCount(uint32_t(record.code.size()));
            if (!record.code.empty()) std::memcpy(shader->m_code.Ptr(), record.code.data(), record.code.size());
            shader->unk_003C = record.unknown0; shader->unk_0040 = record.unknown1;
            shader->unk_0044 = record.unknown2; shader->unk_0046 = record.unknown3;
            IShaderCreate(shader);
            Require(record.code.empty() || shader->m_valid, "DXVK rejected original D3D9 shader");
        }
    } catch (...) {
        for (auto* shader : created) { shader->m_refCount = 1; ShaderDestroy(shader); }
        throw;
    }
    for (uint32_t i = 0; i < count; ++i) { out[i] = list.Ptr(keys[i].c_str()); ++out[i]->m_refCount; }
}
void CGxDeviceD3d::RetainShader(CGxShader* shader) {
    NativeCheckOwner();
    Require(shader && (shader->m_target == GxSh_Vertex || shader->m_target == GxSh_Pixel) && shader->m_refCount < UINT32_MAX,
            "Invalid D3D9 shader reference");
    bool owned = false;
    for (auto* entry = m_shaderList[shader->m_target].m_fulllist.Head(); entry;
         entry = m_shaderList[shader->m_target].m_fulllist.Next(entry)) owned |= entry == shader;
    Require(owned && shader->m_refCount, "Foreign/unretained D3D9 shader");
    ++shader->m_refCount;
}
void CGxDeviceD3d::IShaderReload(CGxShader*, const char*, const char*) { Unavailable("D3D9 native shader reload unavailable"); }

// Native optional display enumeration/inference stays explicitly unavailable.
int32_t CGxDevice::AdapterDesktopMode(CGxMonitorMode& mode) {
    auto* device = dynamic_cast<CGxDeviceD3d*>(g_theGxDevicePtr);
    if (!device) return 0;
    device->NativeCheckOwner();
    mode.m_size = {int32_t(device->m_desktopDisplayMode.Width), int32_t(device->m_desktopDisplayMode.Height)};
    mode.m_bpp = device->m_desktopDisplayMode.Format == D3DFMT_R5G6B5 ? 16 : 32;
    mode.m_refreshRate = device->m_desktopDisplayMode.RefreshRate;
    return 1;
}
int32_t CGxDevice::AdapterMonitorModes(TSGrowableArray<CGxMonitorMode>&) { return 0; }
int32_t CGxDevice::AdapterFormats(EGxApi, TSGrowableArray<CGxFormat>&) { return 0; }
int32_t CGxDevice::DeviceAdapterID(uint16_t& vendor, uint16_t& deviceID, uint32_t& high, uint32_t& low) {
    auto* device = dynamic_cast<CGxDeviceD3d*>(g_theGxDevicePtr);
    if (!device) return 0;
    device->NativeCheckOwner();
    D3DADAPTER_IDENTIFIER9 identity{};
    if (FAILED(device->m_d3d->GetAdapterIdentifier(0, 0, &identity)) || identity.VendorId > UINT16_MAX || identity.DeviceId > UINT16_MAX) return 0;
    vendor = uint16_t(identity.VendorId); deviceID = uint16_t(identity.DeviceId);
    high = uint32_t(identity.DriverVersion.HighPart); low = identity.DriverVersion.LowPart;
    return 1;
}
int32_t CGxDevice::DeviceAdapterInfer(uint16_t&) { return 0; }

// Key functions for original abstract interfaces; keep strict vptr checking.
GxuDrawListItem::~GxuDrawListItem() = default;
GxuDrawListState::~GxuDrawListState() = default;
