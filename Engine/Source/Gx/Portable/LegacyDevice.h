#ifndef WOWDECOMP_NATIVE_LEGACY_DEVICE_H
#define WOWDECOMP_NATIVE_LEGACY_DEVICE_H
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "GlBuffers.h"
#include "GlProgram.h"
#include "GlTransforms.h"
#include <functional>
#include <stdexcept>
#include <vector>

namespace Gx { namespace Native {
// Concrete native ABI adapter for the original Gx dispatch/cache and UI batches.
// The caller retains the SDL context and authenticated shader bytes. The provider
// must only deliver completed reads on this thread; no SFile/ticket casts occur.
// Single installed device, main thread/current context; destroy consumers first.
class LegacyDevice final : public CGxDevice {
public:
    using ShaderBytes = std::function<std::vector<uint8_t>(const std::string&)>;
    LegacyDevice(GlContext& context, ShaderBytes shaders);
    ~LegacyDevice() override;
    void ITexMarkAsUpdated(CGxTex*) override;
    void IRsSendToHw(EGxRenderState) override;
    void ScenePresent() override;
    void SceneClear(uint32_t, CImVector) override;
    void Draw(CGxBatch*, int32_t) override;
    void PoolSizeSet(CGxPool*, uint32_t) override;
    void NativeBufDestroy(CGxBuf*) override;
    void NativeSyncTransforms() override;
    char* BufLock(CGxBuf*) override;
    int32_t BufUnlock(CGxBuf*, uint32_t) override;
    void BufData(CGxBuf*, const void*, uint32_t, uint32_t) override;
    int32_t TexCreate(EGxTexTarget, uint32_t, uint32_t, uint32_t, EGxTexFormat,
        EGxTexFormat, CGxTexFlags, void*, GxTexUserFunc, const char*, CGxTex*&) override;
    void TexDestroy(CGxTex*) override;
    void DeviceSetRenderTarget(EGxBuffer, CGxTex*, uint32_t) override;
    void ShaderCreate(CGxShader**, EGxShTarget, const char*, const char*, uint32_t) override;
    void ShaderDestroy(CGxShader*&) override;
    void IShaderCreate(CGxShader*) override;
    void IShaderReload(CGxShader*, const char*, const char*) override;
    void DeviceReadPixels(CiRect&, TSGrowableArray<CImVector>&) override;
    void CapsWindowSize(CRect&) override;
    void CapsWindowSizeInScreenCoords(CRect&) override;
    void* DeviceWindow() override;
    int32_t DeviceWM(EGxWM, uintptr_t, uintptr_t) override;
    bool StereoEnabled() override { return false; }
    // Unavailable operations fail visibly, including inherited no-op base hooks.
    static void Unavailable(const char* operation) { throw std::logic_error(operation); }
    int32_t DeviceCreate(GxWindowProc, const CGxFormat&) override { Unavailable("Native context is caller-owned"); return 0; }
    int32_t DeviceCreate(void*, const CGxFormat&) override { Unavailable("Native context is caller-owned"); return 0; }
    void DeviceDestroy() override { Unavailable("Destroy the native device before its context"); }
    int32_t DeviceSetFormat(const CGxFormat&) override { Unavailable("Native format recreation unavailable"); return 0; }
    void DeviceEvictResources() override { Unavailable("Native device eviction unavailable"); }
    void DeviceSetGamma(float) override { Unavailable("Native display gamma unavailable"); }
    void DeviceSetGamma(const CGxGammaRamp&) override { Unavailable("Native display gamma unavailable"); }
    void DeviceSaveDepthSurface() override { Unavailable("Native depth save unavailable"); }
    void DeviceRestoreDepthSurface() override { Unavailable("Native depth restore unavailable"); }
    void DeviceResolveDepthBuffer(CGxTex*) override { Unavailable("Native depth resolve unavailable"); }
    void DeviceCopyTex(CGxTex*, uint32_t, CGxTex*, uint32_t) override { Unavailable("Native texture copy unavailable"); }
    void DeviceOverride(EGxOverride, uint32_t) override { Unavailable("Native device overrides unavailable"); }
    void DeviceReadDepths(CiRect&, TSGrowableArray<float>&) override { Unavailable("Native depth readback unavailable"); }
    int32_t TexCopy(CGxTex*, const C2iVector&, const C2iVector&, const C2iVector&, uint32_t, uint32_t) override { Unavailable("Native texture copy unavailable"); return 0; }
    int32_t TexStretch(CGxTex*, const CiRect*, const CiRect*, uint32_t, uint32_t) override { Unavailable("Native texture stretch unavailable"); return 0; }
    bool QueryBegin(CGxQuery*) override { Unavailable("Native queries unavailable"); return false; }
    bool QueryEnd(CGxQuery*) override { Unavailable("Native queries unavailable"); return false; }
    bool QueryGetParam(CGxQuery*, EGxQueryParam, uint32_t&) override { Unavailable("Native queries unavailable"); return false; }
    bool QueryGetData(CGxQuery*, uint32_t&) override { Unavailable("Native queries unavailable"); return false; }
    void PrimPointSize(float) override { Unavailable("Native point parameters unavailable"); }
    void PrimLineWidth(float) override { Unavailable("Native line parameters unavailable"); }
    void StereoSetConvergence(float) override { Unavailable("Native stereo unavailable"); }
    float StereoGetConvergence() override { Unavailable("Native stereo unavailable"); return 0; }
    void StereoSetSeparation(float) override { Unavailable("Native stereo unavailable"); }
    float StereoGetSeparation() override { Unavailable("Native stereo unavailable"); return 0; }
    void DeviceUnk81(uint32_t, uint32_t) override { Unavailable("Unknown native device operation 81"); }
    void DeviceUnk82() override { Unavailable("Unknown native device operation 82"); }
    void DeviceUnk83(uint32_t, uint32_t) override { Unavailable("Unknown native device operation 83"); }
private:
    struct State;
    std::unique_ptr<State> state;
    GlBuffer& Buffer(CGxBuf*);
    void Sync();
    void Check() const;
};
}}
#endif
