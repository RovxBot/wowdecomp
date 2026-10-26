// Authored regression cases for the original model factory and relocation path.
#include "Engine/Source/Frame/Portable/LoginWidgets.h"
#include "Engine/Source/Frame/Portable/Scripts.h"
#include "Engine/Source/Frame/Portable/Fonts.h"
#include "Engine/Source/Frame/Portable/Render.h"
#include "Engine/Source/Frame/CSimpleTop.h"
#include "Engine/Source/Frame/CSimpleModel.h"
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
#include "Engine/Source/Os/Portable/ModelMemory.h"
#include "Engine/Source/Services/Portable/AsyncFile.h"
#include "Engine/Source/Services/Portable/Texture.h"
#include "Engine/Source/Services/Camera.h"
#include "Engine/Source/Services/CameraInt.h"
#include "Engine/Source/Base/Handle.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/Gx/Gx.h"
#include <cstring>
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
    Settings() { ConsoleInitializeClientCommand(); CVar::Initialize(nullptr); }
    ~Settings() { CVar::Destroy(); ConsoleCommandDestroy(); }
};
void Factory() {
    using namespace Frame::Native;
    using namespace Services::Native;
    FileQueue queue;
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
    CoordinateSetAspectRatio(4.0f / 3.0f);
    Runtime layout;
    Gxu::Native::LegacyFonts gxu([](const std::string&) -> Bytes { throw std::runtime_error("Fixture has no font"); });
    Fonts fonts; Renderer renderer;
    TextureService images(queue, [](const std::string&, const Cancellation&) -> ReadResult {
        throw std::runtime_error("Fixture has no images");
    });
    Scripts scripts;
    LoginWidgets widgets;
    {
        CSimpleTop root;
        const char* document = "<ModelFFX name=\"NativeModelBoundary\"><Size x=\"64\" y=\"64\"/></ModelFFX>";
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
        const auto errors = FrameScript::Native::ScriptErrorCount();
        FrameScript_Execute("NativeModelBoundary:ClearModel(); NativeModelBoundary:SetCamera(0); NativeModelBoundary:SetSequence(0); NativeModelBoundary:SetGlow(0.3); assert(NativeModelBoundary:GetModel()==nil); assert(not pcall(function() NativeModelBoundary:SetCamera(-1) end)); assert(not pcall(function() NativeModelBoundary:SetSequence(1e30) end)); assert(not pcall(function() NativeModelBoundary:SetSequenceTime(0,1e30) end)); assert(not pcall(function() NativeModelBoundary:SetGlow(2) end))", "native model contract", nullptr);
        Check(FrameScript::Native::ScriptErrorCount() == errors, "Original model Lua methods failed");
        Check(!model->m_model && model->m_pendingCameraIndex == 0, "Empty model state changed");
        auto* camera = CameraCreate();
        CameraSetFloat(camera, 2, 1000.0f);
        auto* copy = CameraDuplicate(camera);
        HandleClose(reinterpret_cast<HOBJECT>(camera));
        Check(CameraGetFloat(copy, 2) == 1000.0f, "Camera duplicate lost original managed values");
        HandleClose(reinterpret_cast<HOBJECT>(copy));
        model->GetScene();
    }
    scripts.CheckCanClose(); renderer.CheckCanClose(); fonts.CheckCanClose();
}
}
void CheckNativeModels() {
    Relocations();
    for (unsigned cycle = 0; cycle < 3; ++cycle) Factory();
    Check(!g_modelPool && !OsNative::LiveModelAllocations(), "Original model shutdown retained owners");
    std::cout << "native-original-model-factory-camera-relocation-rollback=passed\n";
}
