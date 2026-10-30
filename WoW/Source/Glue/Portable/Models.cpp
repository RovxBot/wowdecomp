// Original Model2/Frame/FFX callers in the qualified Linux native ABI.
#include "Models.h"
#include "Engine/Source/Frame/Portable/Storage.h"
#include "Engine/Source/Frame/Portable/Scripts.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameXML/Portable/Factories.h"
#include "Engine/Source/Frame/CSimpleModel.h"
#include "WoW/Source/Glue/CSimpleModelFFX.h"
#include "WoW/Source/Glue/CGlueMgr.h"
#include "Engine/Source/Model2/M2Cache.h"
#include "Engine/Source/ShaderEffect/ShaderEffect.h"
#include "Engine/Source/ShaderEffect/ShaderEffectManager.h"
#include "Engine/Source/FFX/EffectGlow.h"
#include "Engine/Source/FFX/FFX.h"
#include "Engine/Source/Base/Prop.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/BlizzardCore/Portable/Assets.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "WoW/Source/ObjectAlloc/ObjectAlloc.h"
#include "WoW/Source/DB/Rec/AnimationDataRec.h"
#include "WoW/Source/SoundInterface/SoundInterface2.h"
#include <algorithm>
#include <map>
#include <memory>
#include <thread>
#include <iostream>

FFX::Effect* CGlueMgr::m_glowEffect;
namespace GlueNative {
namespace {
bool initialized = false;
std::map<uint32_t, AnimationDataRec> animations;
std::map<uint32_t, std::string> animationNames;
bool animationsLoaded = false;
void CloseModels() {
    // Frames and their callbacks have already closed inside LoginWidgets.
    GxRsSet(GxRs_VertexShader, static_cast<void*>(nullptr));
    GxRsSet(GxRs_PixelShader, static_cast<void*>(nullptr));
    FFX::SetEffect(nullptr);
    delete CGlueMgr::m_glowEffect; CGlueMgr::m_glowEffect = nullptr;
    FFX::Destroy();
    M2Destroy();
    CShaderEffectManager::DestroyAllEffects();
    CShaderEffect::s_curEffect = nullptr;
    ObjectAllocDestroy(); PropDestroy();
    animations.clear(); animationNames.clear(); animationsLoaded = false;
    initialized = false;
}
void EnsureModels() {
    Frame::Native::RequireStateMutation();
    if (initialized) return;
    PropInitialize();
    // The current single-owner adapter uses no Model2 animation worker.
    // Original CM2Shared::Initialize reserves 32 constants, then three per
    // bone. Its smallest GPU skin profile needs 21 bones (95 constants).
    // Reuse original CPU skinning when the real device cannot fit that profile;
    // UI/FFX shader support alone does not establish GPU skinning support.
    const auto constants = g_theGxDevicePtr->Caps().m_shaderConstants[GxSh_Vertex];
    M2SetCacheFlags(0); // Original cache flags survive M2Destroy; this owner does not.
    M2Initialize(constants >= 95 ? 8 : 0, 64);
    std::cout << "native-model-skinning=" << ((M2GetCacheFlags() & 8) ? "gpu" : "cpu")
              << " vertex-constants=" << constants << std::endl;
    initialized = true;
    try {
        // ShaderEffect also selects vertex ownership, indices and fixed materials.
        // Keep it consistent with the original cache, independently of UI/FFX shaders.
        CShaderEffect::InitShaderSystem((M2GetCacheFlags() & 8) != 0, 0);
        // Original World initialization loads these before any model scene.
        // The retained SFile adapter supplies authenticated, unchanged bytes.
        CShaderEffectManager::AddEffectFile("Model2.wfx");
        CShaderEffectManager::AddEffectFile("Particle.wfx");
        for (const auto* name : {"Particle", "Particle_Unlit", "Projected_ModMod", "Projected_ModAdd",
            "Projected_ModMod_Unlit", "Projected_ModAdd_Unlit"})
            if (!CShaderEffectManager::GetEffect(name))
                throw std::runtime_error(std::string("Missing required original model effect: ")+name);
        if (!CVar::LookupRegistered("gxMultisample"))
            CVar::Register("gxMultisample", "Native surface sample count", 0, "1", nullptr, 0, false, nullptr, false);
        FFX::Init();
        CGlueMgr::m_glowEffect = new EffectGlow;
    } catch (...) { CloseModels(); throw; }
}
CSimpleFrame* Model(CSimpleFrame* parent) {
    EnsureModels();
    return Frame::Native::ConstructRegion<CSimpleModel>(CSimpleModel::s_allocator, parent);
}
CSimpleFrame* ModelFFX(CSimpleFrame* parent) {
    EnsureModels();
    return CSimpleModelFFX::Create(parent);
}
}
void RegisterModels() {
    CSimpleModel::CreateScriptMetaTable();
    CSimpleModelFFX::CreateScriptMetaTable();
    FrameXML::Native::RegisterLoginFactory("Model", Model);
    FrameXML::Native::RegisterLoginFactory("ModelFFX", ModelFFX);
}
void UnregisterModels() {
    if (initialized) CloseModels();
    luaL_unref(FrameScript_GetContext(), LUA_REGISTRYINDEX, CSimpleModelFFX::s_metatable);
    CSimpleModelFFX::s_metatable = LUA_NOREF;
    CSimpleModel::DestroyScriptMetaTable();
}
AnimationDataRec* AnimationRecord(uint32_t id) {
    Frame::Native::RequireOwner();
    if (!initialized) throw std::logic_error("Native Model2 is not initialized");
    if (!animationsLoaded) {
        SFileImpl* file = nullptr;
        if (!SFileOpenFile("DBFilesClient\\AnimationData.dbc", &file))
            throw std::runtime_error("Original AnimationData.dbc is unavailable");
        struct Close { SFileImpl* file; ~Close() { SFileCloseFile(file); } } close{file};
        const auto size = SFileGetFileSize(file, nullptr);
        if (!size || size > 16 * 1024 * 1024) throw std::runtime_error("Invalid AnimationData.dbc size");
        Blizzard::Assets::Bytes bytes(size);
        uint32_t read = 0;
        if (!SFileReadFile(file, bytes.data(), size, &read, nullptr, 0) || read != size)
            throw std::runtime_error("Short AnimationData.dbc read");
        Blizzard::Assets::Dbc dbc(bytes);
        if (dbc.Fields() != 8 || dbc.Rows() > 4096) throw std::runtime_error("Invalid AnimationData.dbc shape");
        std::map<uint32_t, AnimationDataRec> rows;
        std::map<uint32_t, std::string> names;
        for (uint32_t i = 0; i < dbc.Rows(); ++i) {
            const auto key = dbc.UInt(i, 0);
            if (key > INT32_MAX || rows.count(key)) throw std::runtime_error("Invalid AnimationData.dbc identity");
            auto& row = rows[key]; row.m_id = int32_t(key);
            names[key] = dbc.String(i, 1); row.m_name = names[key].c_str();
            row.m_weaponflags = int32_t(dbc.UInt(i, 2)); row.m_bodyflags = int32_t(dbc.UInt(i, 3));
            row.m_flags = int32_t(dbc.UInt(i, 4)); row.m_fallback = int32_t(dbc.UInt(i, 5));
            row.m_behaviorID = int32_t(dbc.UInt(i, 6)); row.m_behaviorTier = int32_t(dbc.UInt(i, 7));
        }
        animations.swap(rows); animationNames.swap(names); animationsLoaded = true;
    }
    auto found = animations.find(id);
    return found == animations.end() ? nullptr : &found->second;
}
}
// Linux x86-64 guarantees SSE; this bounded model source group needs only that
// original feature bit. The remaining OS/CPU feature inventory is not exported.
uint32_t OsGetProcessorFeatures() { return 4; }
uint32_t OsGetNumberOfProcessors() { return std::max(1u, std::thread::hardware_concurrency()); }
// Optional model animation sound events fail explicitly until native audio joins.
int32_t SI2::PlaySoundKit(int32_t, const C3Vector*, SE2SoundKitObject*, SE2SoundKitProperties*,
    uint32_t, uint32_t* soundId, int32_t, int32_t) {
    if (soundId) *soundId = 0;
    std::cout << "native-unavailable=model-animation-audio" << std::endl;
    return 0;
}
