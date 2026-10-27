// Authored regression cases for the original model factory and relocation path.
#include "Engine/Source/Frame/Portable/LoginWidgets.h"
#include "Engine/Source/Frame/Portable/Scripts.h"
#include "Engine/Source/Frame/Portable/Fonts.h"
#include "Engine/Source/Frame/Portable/Render.h"
#include "Engine/Source/Frame/CSimpleTop.h"
#include "Engine/Source/Frame/CSimpleModel.h"
#include "Engine/Source/Frame/CSimpleButton.h"
#include "Engine/Source/Frame/CSimpleHTML.h"
#include "Engine/Source/Frame/CSimpleFont.h"
#include "Engine/Source/Frame/CSimpleMovieFrame.h"
#include "Engine/Source/FrameXML/FrameXML.h"
#include "Engine/Source/FrameXML/XMLTree.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/Portable/Runtime.h"
#include "Engine/Source/Gx/Portable/LegacyDevice.h"
#include "Engine/Source/Gxu/Portable/LegacyFonts.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/ConsoleBase/ConsoleCommand.h"
#include "Engine/Source/Model2/M2Shared.h"
#include "Engine/Source/Model2/M2Cache.h"
#include "Engine/Source/Model2/M2ModelLight.h"
#include "Engine/Source/Model2/Portable/InstanceLayout.h"
#include "Engine/Source/Os/Portable/ModelMemory.h"
#include "Engine/Source/Services/Portable/AsyncFile.h"
#include "Engine/Source/Services/Portable/LegacyFiles.h"
#include "Engine/Source/Services/AsyncFile.h"
#include "Engine/Source/Services/Portable/Texture.h"
#include "Engine/Source/Services/Camera.h"
#include "Engine/Source/Services/CameraInt.h"
#include "Engine/Source/Services/ParticleSystem2.h"
#include "Engine/Source/Base/Handle.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/Gx/Gx.h"
#include "WoW/Source/Glue/CharacterSelection.h"
#include "WoW/Source/Glue/CharacterCreation.h"
#include "WoW/Source/Glue/CSimpleModelFFX.h"
#include "WoW/Source/Glue/Portable/Startup.h"
#include "WoW/Source/Ui/VideoOptions.h"
#include "WoW/Source/SoundInterface/SoundInterface2.h"
#include <cstring>
#include <filesystem>
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
using Bytes = std::vector<uint8_t>;
namespace {
void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
Bytes Program(const char* code, uint32_t count) {
    Bytes result;
    auto word = [&](uint32_t n) { for (unsigned i = 0; i < 4; ++i) result.push_back(uint8_t(n >> (i * 8))); };
    word(0x47585348); word(0x10003); word(0);
    for (uint32_t i = 0; i < count; ++i) {
        word(0); word(0); word(0); word(uint32_t(std::strlen(code) + 1));
        result.insert(result.end(), code, code + std::strlen(code) + 1);
        while (result.size() % 4) result.push_back(0);
    }
    return result;
}
struct ModelBytes {
    static constexpr uint32_t Size = 4096;
    M2Data* data = static_cast<M2Data*>(OsNative::AllocModelRoot(Size, 8192));
    uint32_t used = sizeof(M2Data);
    ModelBytes() {
        Check(data != nullptr, "Authored model root allocation failed");
        std::memset(data, 0, Size); data->m_magic = 0x3032444D; data->m_version = 0x108;
    }
    ~ModelBytes() { OsNative::FreeModelRoot(data); }
    template<class T> T* Array(M2Array<T>& array, uint32_t count) {
        used = (used + alignof(T) - 1) & ~(alignof(T) - 1);
        Check(count <= (Size - used) / sizeof(T), "Authored model overflow");
        array.m_count = count; array.m_offset = used;
        auto* result = reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(data) + used);
        used += sizeof(T) * count; return result;
    }
    bool Relocate() { return M2Init(reinterpret_cast<uint8_t*>(data), Size, *data) != 0; }
};
void Relocations() {
    {
        // Reproduce the former inline-only model ownership: capacity zero can
        // still carry an owned SMem allocation, which the destructor must free.
        CM2Shared shared(nullptr);
        shared.m_lowPrioritySequenceBuffers = static_cast<uint8_t**>(SMemAlloc(0, __FILE__, __LINE__, 8));
        Check(shared.m_lowPrioritySequenceBuffers && !shared.m_lowPrioritySequenceCapacity,
            "Authored zero-capacity sequence owner was not created");
    }
    {
        CPlaneParticleEmitter emitter;
        CPlaneParticleEmitter copy(emitter, 0);
        Check(emitter.m_emitterType==CParticleEmitter2::PET_PLANE_EMITTER &&
            copy.m_emitterType==emitter.m_emitterType && !copy.m_childEmitterCount &&
            !copy.m_model && !copy.m_childModel && !copy.m_texture,
            "Original particle construction/copy created unexpected resources");
    }
    {
        Model2Native::InstanceLayout layout;
        const auto word = layout.Reserve<uint32_t>(1);
        const auto light = layout.Reserve<M2ModelLight>(1);
        const auto pointer = layout.Reserve<CTexture*>(3);
        struct Storage {
            void* bytes;
            ~Storage() { SMemFree(bytes, __FILE__, __LINE__, 0); }
        } storage{SMemAlloc(layout.Size(), __FILE__, __LINE__, 0)};
        auto* bytes = static_cast<uint8_t*>(storage.bytes);
        Check(word==0 && light >= sizeof(uint32_t) &&
            reinterpret_cast<uintptr_t>(bytes+light)%alignof(M2ModelLight)==0 &&
            reinterpret_cast<uintptr_t>(bytes+pointer)%alignof(CTexture*)==0 &&
            pointer+3*sizeof(CTexture*)==layout.Size(), "Native instance arrays lost alignment or allocation coverage");
        auto* object = new (bytes+light) M2ModelLight();
        object->~M2ModelLight();
        bool rejected=false;
        try { layout.Reserve<uint64_t>(UINT32_MAX); } catch (const std::length_error&) { rejected=true; }
        Check(rejected, "Native instance allocation truncated its original byte bound");
    }
    {
        ModelBytes bytes;
        CM2Shared shared(nullptr);
        shared.m_data = bytes.data;
        struct Detach { CM2Shared& shared; ~Detach() { shared.m_data = nullptr; } } detach{shared};
        // The original lookup must reject the observed 20-bone GPU budget;
        // admitting a profile here would hide an unsupported device capability.
        shared.m_data->m_numSkinProfiles = 4;
        Check(shared.GetSkinProfileIndex(20)==UINT32_MAX && shared.GetSkinProfileIndex(21)==3 &&
            shared.GetSkinProfileIndex(53)==2 && shared.GetSkinProfileIndex(256)==0,
            "Original skin-profile register boundaries changed");
        shared.m_data->m_numSkinProfiles = 1;
        Check(shared.GetSkinProfileIndex(20)==UINT32_MAX && shared.GetSkinProfileIndex(256)==0,
            "Original single skin-profile selection changed");
    }
    {
        ModelBytes bytes;
        std::memcpy(bytes.Array(bytes.data->m_name, 4), "m2\0", 4);
        auto* sequence = bytes.Array(bytes.data->m_sequences, 1);
        sequence->m_id = 0; sequence->m_flags = 0x21;
        auto* bone = bytes.Array(bytes.data->m_bones, 1);
        bone->m_translationTrack.m_loopIndex = bone->m_rotationTrack.m_loopIndex = bone->m_scaleTrack.m_loopIndex = UINT16_MAX;
        auto* times = bytes.Array(bone->m_translationTrack.m_sequenceTimes, 1);
        bytes.Array(*times, 1)[0] = 17;
        auto* keys = bytes.Array(bone->m_translationTrack.m_sequenceKeys, 1);
        bytes.Array(*keys, 1)[0] = C3Vector(1, 2, 3);
        Check(bytes.Relocate(), "Original M2Init rejected authored inline tracks");
        Check(!std::strcmp(bytes.data->m_name.Data(), "m2") &&
            bytes.data->m_bones[0].m_translationTrack.m_sequenceTimes[0][0] == 17 &&
            bytes.data->m_bones[0].m_translationTrack.m_sequenceKeys[0][0].y == 2 &&
            (bytes.data->m_sequences[0].m_flags & 0x81) == 0x80,
            "Original relative-track/sequence transformation changed");
    }
    for (unsigned which = 0; which < 4; ++which) {
        ModelBytes bytes;
        if (which == 0) { bytes.data->m_vertices.m_offset = UINT32_MAX; bytes.data->m_vertices.m_count = UINT32_MAX; }
        else if (which == 1) { bytes.data->m_vertices.m_offset = 313; bytes.data->m_vertices.m_count = 1; }
        else {
            if (which == 2) bytes.Array(bytes.data->m_sequences, 1)->m_flags = 0x20;
            auto* bone = bytes.Array(bytes.data->m_bones, 1);
            bone->m_translationTrack.m_loopIndex = UINT16_MAX;
            auto* times = bytes.Array(bone->m_translationTrack.m_sequenceTimes, 1);
            if (which == 2) { times[0].m_count = UINT32_MAX; times[0].m_offset = UINT32_MAX; }
            // No corresponding sequence exists: reject before indexing it.
        }
        Check(!bytes.Relocate(), "Malformed original model array/track was admitted");
    }
    Check(!OsNative::LiveModelAllocations(), "M2 relocation retained an arena");
}
struct Settings {
    std::filesystem::path directory = std::filesystem::temp_directory_path() /
        ("wow-native-model-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::string config = (directory / "Config.wtf").string();
    Settings() { std::filesystem::create_directory(directory); ConsoleInitializeClientCommand(); CVar::Initialize(config.c_str()); }
    ~Settings() { CVar::Destroy(); ConsoleCommandDestroy(); std::filesystem::remove_all(directory); }
};
void Factory() {
    using namespace Frame::Native;
    using namespace Services::Native;
    FileQueue queue;
    // The real cache/model/shared callbacks must report failed initialization
    // and retain their arena through owner-side cleanup. This is deliberately
    // an authored invalid skin-profile declaration, not a rendering fixture.
    unsigned modelReads = 0;
    LegacyFiles modelFiles(queue, [&](const std::string& name, const Cancellation&) {
        Check(name == "NATIVE\\EMPTY.M2", "Unexpected authored model member");
        ++modelReads;
        ReadResult result; result.file.bytes.resize(sizeof(M2Data), 0);
        auto word = [&](size_t offset, uint32_t value) {
            for (unsigned i = 0; i < 4; ++i) result.file.bytes[offset + i] = uint8_t(value >> (i * 8));
        };
        word(0, 0x3032444D); word(4, 0x108);
        return result;
    });
    FrameScript::Native::Runtime vm([](FrameScript::Native::SourceKind, const std::string&, Bytes&) { return false; });
    FrameXML::Native::Runtime xml;
    Settings settings;
    Gx::Native::GlContext context("authored original model contracts", 320, 240, true);
    bool failShader = true;
    Gx::Native::LegacyDevice device(context, [&](const std::string& path) {
        if (path.find("FFXGauss4") != std::string::npos && failShader)
            throw std::runtime_error("Injected FFX construction failure");
        const bool vertex = path.find("Vertex") != std::string::npos;
        const auto count = path.find("FFXGlowWave") != std::string::npos ? 4u :
            path.find("FFX") != std::string::npos || (vertex && path.find("UI.bls") != std::string::npos) ? 2u : 1u;
        return Program(vertex ? "!!ARBvp1.0\nMOV result.position, vertex.position;\nEND\n" :
            "!!ARBfp1.0\nMOV result.color, fragment.color;\nEND\n", count);
    });
    auto* stream = device.BufStream(GxPoolTarget_Vertex, 24, 4);
    Check(device.BufStream(GxPoolTarget_Vertex, 0, 0) == stream && stream->m_itemCount == 4,
        "Original stream identity query changed the active allocation");
    CoordinateSetAspectRatio(4.0f / 3.0f);
    Runtime layout(GlueNative::FrameLevelLimit);
    Gxu::Native::LegacyFonts gxu([](const std::string&) -> Bytes { throw std::runtime_error("Fixture has no font"); });
    Fonts fonts; Renderer renderer;
    TextureService images(queue, [](const std::string&, const Cancellation&) -> ReadResult {
        throw std::runtime_error("Fixture has no images");
    });
    Scripts scripts;
    LoginWidgets widgets;
    CGVideoOptions::RegisterScriptFunctions();
    SI2::RegisterScriptFunctions();
    struct SoundScripts { ~SoundScripts() { SI2::UnregisterScriptFunctions(); } } soundScripts;
    struct VideoScripts { ~VideoScripts() { CGVideoOptions::UnregisterScriptFunctions(); } } videoScripts;
    {
        CSimpleTop root;
        {
            const auto errors = FrameScript::Native::ScriptErrorCount();
            FrameScript_Execute(R"(
                assert(Sound_GameSystem_GetNumOutputDrivers()==0 and Sound_GameSystem_GetNumInputDrivers()==0)
                assert(Sound_ChatSystem_GetNumOutputDrivers()==0 and Sound_ChatSystem_GetNumInputDrivers()==0)
                assert(Sound_GameSystem_GetOutputDriverNameByIndex(0)=='' and Sound_ChatSystem_GetInputDriverNameByIndex(0)=='')
                assert(not pcall(Sound_GameSystem_GetOutputDriverNameByIndex,1e30))
                assert(not pcall(Sound_ChatSystem_GetInputDriverNameByIndex,{}))
                assert(PlaySoundFile==nil and Sound_GameSystem_RestartSoundSystem==nil)
                assert(select('#',PlaySound('authored UI sound'))==0 and select('#',PlaySound(12.5))==0)
                assert(not pcall(PlaySound,{}) and not pcall(PlaySound,1e300) and not pcall(PlaySound,0/0))
                assert(select('#',GetMultisampleFormats()) == 3)
                local color,depth,samples=GetMultisampleFormats()
                assert(color==24 and depth==24 and samples==1 and GetCurrentMultisampleFormat()==1)
                assert(select('#',GetVideoCaps()) == 7 and IsStereoVideoAvailable()==nil)
                assert(GetGamma()==0 and GetTerrainMip()==0)
                assert(select('#',SetGamma(GetGamma()))==0 and select('#',SetTerrainMip(GetTerrainMip()))==0)
                assert(not pcall(SetGamma, 0.5) and GetGamma()==0)
                assert(not pcall(SetGamma, {}) and not pcall(SetGamma, 0/0))
                assert(not pcall(SetTerrainMip, 1) and GetTerrainMip()==0)
                assert(not pcall(SetTerrainMip, -2147483647) and not pcall(SetTerrainMip, 1e30))
                assert(not pcall(SetTerrainMip, {}) and not pcall(SetTerrainMip, 0/0))
                assert(select('#',PlayGlueMusic('authored music'))==0)
                assert(select('#',PlayGlueAmbience('authored ambience',4))==0)
                assert(select('#',StopGlueMusic())==0 and select('#',StopGlueAmbience())==0)
                assert(select('#',StopAllSFX(1))==0 and select('#',StopAllSFX())==0)
                assert(not pcall(StopAllSFX,0/0) and not pcall(StopAllSFX,1e300))
                assert(not pcall(PlayGlueMusic,{}) and not pcall(PlayGlueAmbience,{}))
                assert(not pcall(PlayGlueAmbience,'authored ambience',1e300))
                assert(not pcall(PlayGlueAmbience,'authored ambience',0/0))
                local a,b=GetScreenResolutions(); assert(a=='800x600' and b=='1024x768')
                assert(select('#',GetRefreshRates(1e30))==0 and select('#',GetRefreshRates(-1))==0)
            )", "original video capability query contracts", nullptr);
            Check(FrameScript::Native::ScriptErrorCount()==errors && device.Caps().m_numTmus==1 &&
                device.Caps().m_shaderConstants[GxSh_Vertex]==94, "Video queries changed capabilities or format contracts");
            auto* stereo=CVar::LookupRegistered("gxStereoEnabled");
            auto* farclip=CVar::LookupRegistered("farclip");
            Check(stereo && stereo->m_intValue==0 && farclip && farclip->m_intValue==350,
                "Original graphics preference defaults changed");
            stereo->Set("1",true,false,false,true);
            Check(!device.Caps().m_stereoAvailable && !device.m_format.m_stereoEnabled,
                "An unavailable stereo preference changed actual graphics capabilities");
            stereo->Set("0",true,false,false,true);
        }
        {
            Frame::Native::LoadScriptEdits load;
            Check(CSimpleFont::GetFont("NativeTooltipFont", 1) != nullptr, "Authored tooltip font owner unavailable");
            const char* document = R"(<Frame name="NativeLoginAlignment"><Layers><Layer level="OVERLAY"><FontString name="$parentText"/></Layer></Layers><Frames><Button name="$parentClick"><Scripts><OnLoad>self:RegisterForClicks('LeftButtonDown','LeftButtonUp')</OnLoad></Scripts></Button></Frames><Scripts><OnLoad>
                NativeLoginAlignmentText:SetJustifyH('LEFT'); NativeLoginAlignmentText:SetJustifyV('TOP');
                NativeLoginAlignmentText:SetFontObject('NativeTooltipFont');
                NativeLoginAlignmentText:SetTextColor(1,0,0,.5); NativeLoginAlignmentText:SetVertexColor(0,1,0);
                local r,g,b,a=NativeLoginAlignmentText:GetTextColor(); assert(r==0 and g>.99 and b==0 and a>.49 and .51>a);
                assert(NativeLoginAlignmentText:GetFontObject():IsObjectType('Font'));
                assert(NativeLoginAlignmentText:GetJustifyH() == 'LEFT' and NativeLoginAlignmentText:GetJustifyV() == 'TOP');
                self:SetFrameLevel(20); assert(self:GetFrameLevel() == 20 and NativeLoginAlignmentClick:GetFrameLevel() == 21);
                self:EnableKeyboard(true); self:EnableMouse(true); self:EnableMouseWheel(true);
                assert(self:IsKeyboardEnabled() and self:IsMouseEnabled() and self:IsMouseWheelEnabled());
                self:EnableKeyboard(false); self:EnableMouse(false); self:EnableMouseWheel(false);
                assert(not self:IsKeyboardEnabled() and not self:IsMouseEnabled() and not self:IsMouseWheelEnabled());
                assert(not pcall(self.SetFrameLevel,self,513) and not pcall(self.SetFrameLevel,self,-1));
                assert(not pcall(self.SetFrameLevel,self,1e30) and self:GetFrameLevel() == 20);
                self:SetFrameLevel(200); assert(self:GetFrameLevel() == 148 and NativeLoginAlignmentClick:GetFrameLevel() == 149);
                self:SetFrameLevel(200); assert(self:GetFrameLevel() == 200);
                self:SetFrameLevel(383); self:SetFrameLevel(383); self:SetFrameLevel(511);
                assert(self:GetFrameLevel() == 511 and NativeLoginAlignmentClick:GetFrameLevel() == 512);
                assert(not pcall(self.SetFrameLevel,self,512) and self:GetFrameLevel() == 511);
                self:SetFrameLevel(20);
            </OnLoad></Scripts></Frame>)";
            std::unique_ptr<XMLTree, decltype(&XMLTree_Free)> tree(XMLTree_Load(document, std::strlen(document)), XMLTree_Free);
            CStatus status;
            Check(bool(tree), "Authored login OnLoad XML failed to parse");
            const auto errors = FrameScript::Native::ScriptErrorCount();
            std::unique_ptr<CSimpleFrame> frame(FrameXML_CreateFrame(XMLTree_GetRoot(tree.get()), nullptr, &status));
            Check(frame && FrameScript::Native::ScriptErrorCount() == errors,
                ("Original login OnLoad state contracts failed: " + FrameScript::Native::LastScriptError()).c_str());
            auto* button = static_cast<CSimpleButton*>(CScriptObject::GetScriptObjectByName("NativeLoginAlignmentClick", CSimpleButton::GetObjectType()));
            Check(button && button->m_clickAction == (CLICK_ON_LEFTDOWN | CLICK_ON_LEFTUP),
                "Original character screen OnLoad could not register both click edges");
        }
        {
            const char* document = "<SimpleHTML name=\"NativeOptionalHtml\" file=\"missing.html\"/>";
            std::unique_ptr<XMLTree, decltype(&XMLTree_Free)> tree(XMLTree_Load(document, std::strlen(document)), XMLTree_Free);
            CStatus status;
            std::unique_ptr<CSimpleFrame> frame(FrameXML_CreateFrame(XMLTree_GetRoot(tree.get()), nullptr, &status));
            auto* html = dynamic_cast<CSimpleHTML*>(frame.get());
            Check(html && decltype(html->m_content)::IsEnd(html->m_content.Head()), "Absent optional HTML did not preserve the original empty widget");
        }
        {
            const char* document = R"(<MovieFrame name="NativeInactiveMovie" hidden="true"><Scripts>
                <OnMovieFinished>NativeMovieFinished=true</OnMovieFinished>
                <OnMovieShowSubtitle>NativeMovieSubtitle=text</OnMovieShowSubtitle>
                <OnMovieHideSubtitle>NativeMovieSubtitle=nil</OnMovieHideSubtitle>
            </Scripts></MovieFrame>)";
            std::unique_ptr<XMLTree, decltype(&XMLTree_Free)> tree(XMLTree_Load(document, std::strlen(document)), XMLTree_Free);
            CStatus status;
            std::unique_ptr<CSimpleFrame> frame(FrameXML_CreateFrame(XMLTree_GetRoot(tree.get()), nullptr, &status));
            auto* movie = dynamic_cast<CSimpleMovieFrame*>(frame.get());
            Check(movie && !movie->m_isPlaying && !movie->m_sound.m_internal && movie->m_onMovieFinished.m_luaRef,
                "Original inactive MovieFrame identity or callback ownership failed");
            const auto errors = FrameScript::Native::ScriptErrorCount();
            FrameScript_Execute(R"(
                assert(NativeInactiveMovie:IsObjectType('MovieFrame') and NativeInactiveMovie:HasScript('OnMovieShowSubtitle'))
                NativeInactiveMovie:EnableSubtitles(true)
                assert(NativeInactiveMovie:StartMovie('unavailable-native-movie', 100) == nil)
                assert(not pcall(NativeInactiveMovie.StartMovie,NativeInactiveMovie,'invalid',1e30))
                NativeInactiveMovie:StopMovie(); assert(NativeMovieFinished == nil)
            )", "native movie explicit unavailability", nullptr);
            Check(FrameScript::Native::ScriptErrorCount() == errors && movie->m_subtitlesEnabled && !movie->m_isPlaying,
                "Native MovieFrame fabricated playback or a finished callback");
        }
        // An unavailable image must unwind the HTML parser's borrowed tree;
        // otherwise XML owner shutdown terminates and hides the real error.
        {
            CSimpleHTML html(nullptr);
            const char* text = "<HTML><BODY><IMG src=\"unavailable.png\" width=\"16\" height=\"16\"/></BODY></HTML>";
            bool failed = false;
            try { html.SetText(text, nullptr); } catch (const std::invalid_argument&) { failed = true; }
            Check(failed, "HTML failure fixture did not reach image validation");
        }
        const char* document = "<ModelFFX name=\"NativeModelBoundary\" glow=\"0.25\" fogNear=\"0.1\" fogFar=\"100\"><Size x=\"64\" y=\"64\"/><FogColor r=\"1\" g=\"0\" b=\"0\" a=\"1\"/><Scripts><OnUpdateModel>NativeModelUpdated=self</OnUpdateModel><OnAnimFinished>NativeModelFinished=self</OnAnimFinished></Scripts></ModelFFX>";
        std::unique_ptr<XMLTree, decltype(&XMLTree_Free)> tree(XMLTree_Load(document, std::strlen(document)), XMLTree_Free);
        Check(bool(tree), "Authored model XML failed to parse");
        CStatus status;
        bool rejected = false;
        try { delete FrameXML_CreateFrame(XMLTree_GetRoot(tree.get()), nullptr, &status); }
        catch (const std::runtime_error&) { rejected = true; }
        Check(rejected && !g_modelPool && !OsNative::LiveModelAllocations(), "Partial FFX setup leaked model ownership");
        failShader = false;
        auto* model = dynamic_cast<CSimpleModel*>(FrameXML_CreateFrame(XMLTree_GetRoot(tree.get()), nullptr, &status));
        Check(model != nullptr, "Real original ModelFFX factory unavailable");
        Check(!(M2GetCacheFlags() & (8 | 4)) && device.Caps().m_shaderConstants[GxSh_Vertex]==94 &&
            device.Caps().m_numTmus==1, "Original CPU skinning changed device limits or enabled a worker");
        const auto errors = FrameScript::Native::ScriptErrorCount();
        Check(model->m_onUpdateModel.m_luaRef && model->m_onAnimFinished.m_luaRef, "Original model XML callbacks were not retained");
        model->RunScript(model->m_onUpdateModel, 0, nullptr);
        model->RunScript(model->m_onAnimFinished, 0, nullptr);
        FrameScript_Execute("assert(NativeModelUpdated == NativeModelBoundary and NativeModelFinished == NativeModelBoundary)",
            "authored model callback dispatch", nullptr);
        {
            const char* invalid = "<Frame name=\"RejectedModelHandler\"><Scripts><OnUpdateModel>error('unreachable')</OnUpdateModel></Scripts></Frame>";
            std::unique_ptr<XMLTree, decltype(&XMLTree_Free)> invalidTree(XMLTree_Load(invalid, std::strlen(invalid)), XMLTree_Free);
            bool rejected = false;
            try { delete FrameXML_CreateFrame(XMLTree_GetRoot(invalidTree.get()), nullptr, &status); }
            catch (const std::invalid_argument&) { rejected = true; }
            Check(rejected, "Model-specific script handler accepted a base Frame parent");
        }
        CharSelectRegisterScriptFunctions(); CharacterCreateRegisterScriptFunctions();
        FrameScript_Execute(R"(
            SetCharSelectModelFrame('NativeModelBoundary'); SetCharCustomizeFrame('NativeModelBoundary')
            SetCharSelectModelFrame('MissingModelFrame'); SetCharCustomizeFrame('MissingModelFrame')
            assert(not pcall(SetCharSelectModelFrame, {}))
            assert(not pcall(SetCharCustomizeFrame, {}))
        )", "original character model frame binding", nullptr);
        Check(CCharacterSelection::m_modelFrame == model && CCharacterCreation::m_charCustomizeFrame == model,
            "Original character consumers did not retain their actual ModelFFX frame");
        CharSelectUnregisterScriptFunctions(); CharacterCreateUnregisterScriptFunctions();
        Check(!CCharacterSelection::m_modelFrame && !CCharacterCreation::m_charCustomizeFrame,
            "Original character frame bindings survived native unregistration");
        Check((model->m_flags & CSimpleModel::MODEL_FOGGED) && model->m_fogColor.r == 255 &&
            model->m_fogColor.g == 0 && model->m_fogNear == 0.1f && model->m_fogFar == 100.0f,
            "Original model XML fog handler did not run");
        FrameScript_Execute("NativeModelBoundary:ClearModel(); NativeModelBoundary:SetCamera(0); NativeModelBoundary:SetSequence(0); NativeModelBoundary:SetGlow(0.3); assert(NativeModelBoundary:GetModel()==nil); assert(not pcall(function() NativeModelBoundary:SetCamera(-1) end)); assert(not pcall(function() NativeModelBoundary:SetSequence(1e30) end)); assert(not pcall(function() NativeModelBoundary:SetSequenceTime(0,1e30) end)); assert(not pcall(function() NativeModelBoundary:SetGlow(2) end))", "native model contract", nullptr);
        Check(FrameScript::Native::ScriptErrorCount() == errors, "Original model Lua methods failed");
        Check(!model->m_model && model->m_pendingCameraIndex == 0, "Empty model state changed");
        auto* camera = CameraCreate();
        CameraSetFloat(camera, 2, 1000.0f);
        auto* copy = CameraDuplicate(camera);
        HandleClose(reinterpret_cast<HOBJECT>(camera));
        Check(CameraGetFloat(copy, 2) == 1000.0f, "Camera duplicate lost original managed values");
        HandleClose(reinterpret_cast<HOBJECT>(copy));
        model->SetModel("native\\empty.m2");
        Check(model->m_model != nullptr, "Original model instance/cache did not start its retained read");
        bool loadFailed = false;
        try { AsyncFileReadWaitAll(); } catch (const std::runtime_error&) { loadFailed = true; }
        Check(loadFailed && modelReads == 1, "Original model initialization failure was hidden");
        model->SetModel(static_cast<CM2Model*>(nullptr));
        modelFiles.CheckCanClose();
        {
            CM2Shared shared(nullptr);
            const std::string longName = std::string(256, 'a') + ".m2";
            std::memcpy(shared.m_fileName, longName.c_str(), longName.size() + 1);
            bool bounded = false;
            try { shared.LoadSkinProfile(0); } catch (const std::length_error&) { bounded = true; }
            Check(bounded, "Native model suffix overflow reached a file operation");
        }
    }
    const auto closeErrors = FrameScript::Native::ScriptErrorCount();
    FrameScript_Execute("retainedGamma=SetGamma; retainedGlueMusic=PlayGlueMusic", "retain option closures", nullptr);
    CGVideoOptions::UnregisterScriptFunctions(); SI2::UnregisterScriptFunctions();
    FrameScript_Execute("assert(SetGamma==nil and PlayGlueMusic==nil); assert(not pcall(retainedGamma,0) and not pcall(retainedGlueMusic,'closed'))",
        "closed option closures", nullptr);
    Check(FrameScript::Native::ScriptErrorCount()==closeErrors, "Option closures survived their native owners");
    scripts.CheckCanClose(); renderer.CheckCanClose(); fonts.CheckCanClose();
}
}
void CheckNativeModels() {
    Relocations();
    for (unsigned cycle = 0; cycle < 3; ++cycle) Factory();
    Check(!g_modelPool && !OsNative::LiveModelAllocations(), "Original model shutdown retained owners");
    std::cout << "native-original-model-factory-camera-relocation-rollback=passed\n";
}
