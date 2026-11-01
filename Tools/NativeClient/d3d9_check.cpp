// Focused regression of the device used by WowNativeClient, with authored bytes.
#include "Engine/Source/Gx/CGxDeviceD3d/CGxDeviceD3d.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Os/Portable/Runtime.h"
#include "Engine/Source/Os/Time.h"
#include "Engine/Source/FrameScript/Portable/Runtime.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameXML/Portable/Runtime.h"
#include "Engine/Source/Gxu/Portable/LegacyFonts.h"
#include "Engine/Source/Frame/Portable/Fonts.h"
#include "Engine/Source/Frame/Portable/Scripts.h"
#include "Engine/Source/Frame/CSimpleTop.h"
#include "Engine/Source/Frame/CSimpleButton.h"
#include "Engine/Source/Frame/CSimpleTexture.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Services/Portable/Texture.h"
#include <SDL3/SDL.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include "ShaderSource.h"

using Bytes = std::vector<uint8_t>;
void CheckNativeScanServices();
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
struct TextureSource {
    CImVector pixels[64];
    TextureSource() { for (auto& p : pixels) p = CImVector(37,83,149,255); }
    unsigned locks = 0, unlocks = 0;
    bool fail = true, badStride = false;
    static void Read(EGxTexCommand command, uint32_t width, uint32_t, uint32_t, uint32_t,
                     void* user, uint32_t& stride, const void*& bytes) {
        auto& source = *static_cast<TextureSource*>(user);
        if (command == GxTex_Lock) ++source.locks;
        if (command == GxTex_Unlock) ++source.unlocks;
        if (command == GxTex_Latch && source.fail) throw std::runtime_error("authored texture callback failure");
        stride = source.badStride ? 1 : width * 4; bytes = source.pixels;
    }
};
void Constants(CGxDeviceD3d& device) {
    const float value[4]{1,2,3,4};
    // Reproduced original AccountLogin M2 palette: c31 through c186.
    auto* palette = device.ShaderConstantsLockRange(GxSh_Vertex, 31, 156);
    Require(palette == CGxDevice::s_shadowConstants[1].constants + 31 * 4, "Original M2 palette span rejected or rebased");
    device.ShaderConstantsSet(GxSh_Vertex, 255, value, 1);
    device.ShaderConstantsSet(GxSh_Pixel, 223, value, 1);
    auto vertex = CGxDevice::s_shadowConstants[1], pixel = CGxDevice::s_shadowConstants[0];
    Reject([&] { device.ShaderConstantsLockRange(GxSh_Vertex, 31, UINT64_MAX); });
    Reject([&] { device.ShaderConstantsLockRange(GxSh_Vertex, 255, 2); });
    Reject([&] { device.ShaderConstantsSet(GxSh_Pixel, 223, value, 2); });
    Reject([&] { device.ShaderConstantsUnlock(GxSh_Vertex, 256, 1); });
    Reject([&] { device.ShaderConstantsLockRange(GxSh_Geometry, 0, 1); });
    Require(!std::memcmp(&vertex, &CGxDevice::s_shadowConstants[1], sizeof(vertex)) &&
            !std::memcmp(&pixel, &CGxDevice::s_shadowConstants[0], sizeof(pixel)),
            "Rejected constant span changed shadow data or dirty bounds");
    const auto actual = device.m_caps.m_shaderConstants[GxSh_Vertex];
    device.m_caps.m_shaderConstants[GxSh_Vertex] = 128;
    Reject([&] { device.ShaderConstantsLockRange(GxSh_Vertex, 31, 156); });
    device.m_caps.m_shaderConstants[GxSh_Vertex] = actual;
}
void DialogRaise() {
    Services::Native::FileQueue queue;
    FrameScript::Native::Runtime vm([](FrameScript::Native::SourceKind, const std::string&, Bytes&) { return false; });
    FrameXML::Native::Runtime xml;
    Frame::Native::Runtime layout;
    Gxu::Native::LegacyFonts gxu([](const std::string&) -> Bytes { throw std::runtime_error("Fixture has no fonts"); });
    Frame::Native::Fonts fonts;
    Frame::Native::Renderer renderer;
    Services::Native::TextureService images(queue, [](const std::string&, const Services::Native::Cancellation&) -> Services::Native::ReadResult {
        throw std::runtime_error("Fixture has no images");
    });
    Frame::Native::Scripts scripts;
    CoordinateSetAspectRatio(4.0f / 3.0f);
    CSimpleTop root;
    const auto errors = FrameScript::Native::ScriptErrorCount();
    auto lua = [&](const char* source) {
        FrameScript_Execute(source, "authored original dialog Raise", nullptr);
        if (FrameScript::Native::ScriptErrorCount()!=errors)
            throw std::runtime_error("Dialog Raise regression failed: " + FrameScript::Native::LastScriptError());
    };
    lua(R"(
        local f=CreateFrame('Frame','RaiseDialog'); f:SetSize(100,100);
        f:SetPoint('CENTER'); f:SetFrameLevel(0); f:Hide();
        local other=CreateFrame('Frame','RaiseOther'); other:SetAllPoints(f); other:SetFrameLevel(1);
    )");
    auto* frame = static_cast<CSimpleFrame*>(CScriptObject::GetScriptObjectByName("RaiseDialog", CSimpleFrame::GetObjectType()));
    auto* other = static_cast<CSimpleFrame*>(CScriptObject::GetScriptObjectByName("RaiseOther", CSimpleFrame::GetObjectType()));
    frame->SetFrameFlag(1,1);
    frame->Resize(1); other->Resize(1);
    CRect rect;
    Require(frame->GetRect(&rect) && other->GetRect(&rect), "Authored occlusion requires resolved rectangles");
    lua(R"(
        RaiseDialog:SetScript('OnShow',function(self)
            local level=self:GetFrameLevel(); assert(level>RaiseOther:GetFrameLevel());
            self:Raise(); self:Raise(); assert(self:GetFrameLevel()==level);
            assert(not pcall(self.SetFrameLevel,self,0));
            assert(not pcall(RaiseOther.Raise,RaiseOther));
            assert(not pcall(CreateFrame,'Frame','ForbiddenDuringShow'));
            raisedOnShow=true;
        end);
        RaiseDialog:Show(); assert(raisedOnShow);
        local level=RaiseDialog:GetFrameLevel(); RaiseDialog:Raise(); assert(RaiseDialog:GetFrameLevel()==level);
        RaiseDialog:SetScript('OnHide',function(self) assert(not pcall(self.Raise,self)) end);
        RaiseDialog:Hide(); RaiseDialog:Show();
    )");
    {
        Frame::Native::CallbackScope callback;
        Reject([&] { Frame::Native::RaiseFromScript(frame); });
    }
    {
        Frame::Native::ResizeScope resize;
        Reject([&] { Frame::Native::RaiseFromScript(frame); });
    }
    lua(R"(
        local b=CreateFrame('Button','UpdateButton'); b:SetSize(10,10); b:SetPoint('CENTER');
        b:SetNormalTexture('fixture-a.blp'); b:SetPushedTexture('fixture-a.blp');
        b:SetHighlightTexture('fixture-a.blp');
        local n,p,h=b:GetNormalTexture(),b:GetPushedTexture(),b:GetHighlightTexture();
        updateTextureCalls=0;
        b:SetScript('OnUpdate',function(self)
            self:SetNormalTexture('fixture-b.blp'); self:SetPushedTexture('fixture-b.blp');
            self:SetHighlightTexture('fixture-b.blp');
            assert(n==self:GetNormalTexture() and p==self:GetPushedTexture() and h==self:GetHighlightTexture());
            assert(not pcall(self.SetDisabledTexture,self,'missing-slot.blp'));
            assert(not pcall(self.SetNormalTexture,self,nil));
            assert(not pcall(self.SetNormalTexture,self,h));
            assert(not pcall(self.CreateTexture,self));
            updateTextureCalls=updateTextureCalls+1;
        end);
    )");
    root.OnLayerUpdate(.016f); root.OnLayerUpdate(.016f);
    lua("assert(updateTextureCalls==2); UpdateButton:SetScript('OnUpdate',nil)");
    auto* button = static_cast<CSimpleButton*>(CScriptObject::GetScriptObjectByName("UpdateButton", CSimpleButton::GetObjectType()));
    const auto* normalImage = button->m_stateTextures[BUTTONSTATE_NORMAL]->m_texture;
    const auto* highlightImage = button->m_highlightTexture->m_texture;
    {
        Frame::Native::ResizeScope resize;
        Reject([&] { button->SetStateTexture(BUTTONSTATE_NORMAL, "fixture-c.blp"); });
    }
    {
        Frame::Native::CollectScope collect;
        Reject([&] { button->SetHighlight("fixture-c.blp", GxBlend_Add); });
    }
    Require(button->m_stateTextures[BUTTONSTATE_NORMAL]->m_texture==normalImage &&
            button->m_highlightTexture->m_texture==highlightImage, "Rejected texture content mutation changed image ownership");
    std::cout << "native-original-dialog-repeat-raise-onshow-no-level-mutation-guards=passed\n";
    std::cout << "native-original-dialog-update-textures-retain-regions-reject-ownership-changes=passed\n";
}
void Pixel(CGxDeviceD3d& device) {
    TextureSource source;
    CGxTex* texture = nullptr;
    Require(GxTexCreate(8, 8, GxTex_Argb8888, CGxTexFlags(GxTex_Linear), &source, TextureSource::Read, texture), "Original texture creation failed");
    struct Cleanup { CGxTex* texture; ~Cleanup() { GxTexDestroy(texture); } } cleanup{texture};
    Reject([&] { GxTexUpdate(texture, 0, 0, 8, 8, 1); });
    Require(source.locks == 1 && source.unlocks == 1 && texture->m_needsUpdate, "Callback failure lost unlock/retry ownership");
    source.fail = false; source.badStride = true;
    Reject([&] { GxTexUpdate(texture, 0, 0, 8, 8, 1); });
    Require(source.locks == 2 && source.unlocks == 2 && texture->m_needsUpdate, "Bad stride lost unlock/retry ownership");
    source.badStride = false;
    GxTexUpdate(texture, 0, 0, 8, 8, 1);
    Require(device.TexIsReadyNow(texture) && source.locks == 3 && source.unlocks == 3, "Original upload retry failed");
    GxRsSet(GxRs_Texture0, texture);
    GxRsSet(GxRs_VertexShader, static_cast<void*>(nullptr));
    GxRsSet(GxRs_PixelShader, static_cast<void*>(nullptr));
    GxRsSet(GxRs_Lighting, 0); GxRsSet(GxRs_Fog, 0); GxRsSet(GxRs_Culling, 0);
    GxRsSet(GxRs_DepthTest, 0); GxRsSet(GxRs_BlendingMode, GxBlend_Opaque);
    C44Matrix identity; identity.Identity();
    GxXformSetProjection(identity); GxXformSetView(identity);
    struct Vertex { float x, y, z; CImVector color; float u, v; };
    const Vertex quad[]{{-1,-1,0,{255,255,255,255},0,0},{1,-1,0,{255,255,255,255},1,0},
                        {1,1,0,{255,255,255,255},1,1},{-1,1,0,{255,255,255,255},0,1}};
    auto* buffer = GxBufStream(GxPoolTarget_Vertex, sizeof(Vertex), 4);
    auto* mapped = GxBufLock(buffer);
    std::memcpy(mapped, quad, sizeof(quad));
    Reject([&] { GxBufUnlock(buffer, sizeof(quad) + 1); });
    Require(device.m_bufLocked[0] == buffer, "Rejected Unlock discarded mapped ownership");
    device.AbortBufferWrite(buffer);
    Require(!device.m_bufLocked[0] && !buffer->m_hasData, "Aborted Frame write retained mapped/published data");
    mapped = GxBufLock(buffer);
    std::memcpy(mapped, quad, sizeof(quad));
    GxBufUnlock(buffer, sizeof(quad));
    GxPrimVertexPtr(buffer, GxVBF_PCT);
    const uint16_t indices[]{0,1,2,0,2,3};
    GxPrimIndexPtr(6, indices);
    CGxBatch batch{GxPrim_Triangles, 0, 6, 0, 3};
    GxSceneClear(GxBufferBit_Color | GxBufferBit_Depth, CImVector(0,0,0,255));
    GxDraw(&batch, 1);
    CiRect rect{0, 0, 4, 4};
    TSGrowableArray<CImVector> pixels;
    device.DeviceReadPixels(rect, pixels);
    Require(pixels.Count() == 16, "Original D3D9 readback extent");
    for (uint32_t i = 0; i < pixels.Count(); ++i) Require(pixels[i].r == 149 && pixels[i].g == 83 && pixels[i].b == 37, "Original Gx texture/draw/readback channel mismatch");
    GxScenePresent();

}
}
int main() {
    if (SDL_setenv_unsafe("DXVK_WSI_DRIVER", "SDL3", 1)) return 1;
    StormRtlInitialize();
    try {
        CheckNativeScanServices();
        using Services::Native::ReadStatus;
        Services::Native::ReadResult read;
        read.status = ReadStatus::Failed; read.error = "MPQ member not found: SHADERS\\PIXEL\\ABSENT.BLS";
        Require(MissingShaderRead(read, "Shaders/Pixel/Absent.bls"), "Exact missing-package disposition rejected");
        for (auto error : {"MPQ file MD5 verification failed: SHADERS\\PIXEL\\ABSENT.BLS", "MPQ member deleted by patch: SHADERS\\PIXEL\\ABSENT.BLS"}) {
            read.error = error; Require(!MissingShaderRead(read, "Shaders/Pixel/Absent.bls"), "Integrity/deletion failure admitted as absent");
        }
        for (auto status : {ReadStatus::Success, ReadStatus::Cancelled}) {
            read.status = status; read.error = "MPQ member not found: SHADERS\\PIXEL\\ABSENT.BLS";
            Require(!MissingShaderRead(read, "Shaders/Pixel/Absent.bls"), "Non-failure status admitted as absent");
        }
        for (unsigned cycle = 0; cycle < 3; ++cycle) {
            OsNative::Window window("Original Gx DXVK client contracts", 320, 240, true, OsNative::WindowGraphics::Vulkan);
            std::cout << "original-gx-stage=provider-failure cycle=" << cycle << std::endl;
            Reject([&] { CGxDeviceD3d failed(window, [](const std::string&) -> Bytes { throw std::runtime_error("authored provider failure"); }); });
            Require(!g_theGxDevicePtr, "Failed shader setup published a device");
            Reject([&] { CGxDeviceD3d failed(window, [](const std::string& path) {
                if (path.find("Pixel") != std::string::npos) throw std::runtime_error("authored partial shader failure");
                return Shader(path);
            }); });
            Require(!g_theGxDevicePtr, "Partial shader setup published a device");
            {
                std::cout << "original-gx-stage=device-create" << std::endl;
                CGxDeviceD3d device(window, Shader);
                Require(g_theGxDevicePtr == &device && device.DeviceApi() == GxApi_D3d9, "Original D3D9 dispatch not installed");
                Require(device.Caps().m_numTmus > 1 && device.Caps().m_shaderConstants[GxSh_Vertex] > 94, "DXVK inherited frozen GL caps");
                Require(!CGxDevice::s_uiVertexShader[0]->m_loaded && CGxDevice::s_uiVertexShader[0]->Valid(),
                        "Original multi-permutation shader creation lost lazy binding");
                Require(!CGxDevice::s_uiVertexShader[1]->m_loaded, "Unused shader permutation compiled eagerly");
                Constants(device);
                bool offThread = false;
                std::thread thread([&] { try { device.ScenePresent(); } catch (const std::exception&) { offThread = true; } });
                thread.join(); Require(offThread, "Off-owner presentation accepted");
                CGxShader* bad = nullptr;
                auto saved = device.m_nativeShaders;
                device.m_nativeShaders = [](const std::string&) { auto bytes = Package(false, 1); bytes[4] = 4; return bytes; };
                Reject([&] { device.ShaderCreate(&bad, GxSh_Pixel, "Shaders\\Pixel", "BadVersion", 1); });
                Require(!bad, "Rejected package published a shader");
                std::vector<std::string> attempts;
                device.m_nativeShaders = [&](const std::string& path) -> std::optional<Bytes> {
                    attempts.push_back(path);
                    if (path.find("ps_1_4") == std::string::npos) return std::nullopt;
                    return Package(false, 1);
                };
                CGxShader* fallback = nullptr;
                device.ShaderCreate(&fallback, GxSh_Pixel, "Shaders\\Pixel", "Fallback", 1);
                Require(fallback->Valid() && attempts.size() == 3 && device.Caps().m_shaderTargets[GxSh_Pixel] == 4,
                        "Original package descent changed backend caps or skipped a profile");
                device.ShaderDestroy(fallback);
                unsigned integrityAttempts = 0;
                device.m_nativeShaders = [&](const std::string&) -> std::optional<Bytes> {
                    ++integrityAttempts; throw std::runtime_error("authored signature failure");
                };
                Reject([&] { device.ShaderCreate(&fallback, GxSh_Pixel, "Shaders\\Pixel", "BadSignature", 1); });
                Require(!fallback && integrityAttempts == 1, "Integrity failure fell through to another shader profile");
                device.m_nativeShaders = saved;
                std::cout << "original-gx-stage=draw" << std::endl;
                Pixel(device);
                if (!cycle) DialogRaise();
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
