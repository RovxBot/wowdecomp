// Original 12340 CSimpleMovieFrame identity, callback and inactive lifecycle
// selected for ordered GlueXML creation. Native playback is unavailable: start
// returns failure explicitly and never produces a decoder, sound or video frame.
#include "Engine/Source/Frame/CSimpleMovieFrame.h"
#include "Engine/Source/Frame/Portable/Storage.h"
#include "Engine/Source/Frame/Portable/Scripts.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Storm/H/storm.h"
#include <exception>
#include <iostream>
#include <stdexcept>
CDataAllocator CSimpleMovieFrame::s_allocator(sizeof(CSimpleMovieFrame), 5);
CSimpleMovieFrame* CSimpleMovieFrame::s_currentMovieFrame;
int32_t CSimpleMovieFrame::s_decoderCount;
int32_t CSimpleMovieFrame::s_metatable = LUA_NOREF;
int32_t CSimpleMovieFrame::s_objectType;
const char* CSimpleMovieFrame::s_objectTypeName = "MovieFrame";
// SoundEngine.cpp: the original unopened SESound has no underlying resource.
// No FMOD implementation, audio API or success-producing replacement is linked.
SESound::SESound() : m_internal(nullptr) {}
SESound::~SESound() { if (m_internal) std::terminate(); }
void CSimpleMovieFrame::operator delete(void* ptr) {
    if (ptr) Frame::Native::FreeRegionStorage(s_allocator, ptr);
}
int32_t CSimpleMovieFrame::GetObjectType() {
    if (!CSimpleMovieFrame::s_objectType) {
        CSimpleMovieFrame::s_objectType = ++FrameScript_Object::s_objectTypes;
    }

    return CSimpleMovieFrame::s_objectType;
}

void CSimpleMovieFrame::RunOnMovieShowSubtitleScript(const char* text) {
    if (this->m_onMovieShowSubtitle.m_luaRef) {
        lua_State* L = FrameScript_GetContext();
        lua_pushstring(L, text);
        this->RunScript(this->m_onMovieShowSubtitle, 1, nullptr);
    }
}

FrameScript_Object::ScriptIx* CSimpleMovieFrame::GetScriptByName(const char* name, FrameScript_Object::ScriptData& data) {
    FrameScript_Object::ScriptIx* script = CSimpleFrame::GetScriptByName(name, data);

    if (script) {
        return script;
    }

    if (!SStrCmpI(name, "OnMovieFinished", STORM_MAX_STR)) {
        return &this->m_onMovieFinished;
    }

    if (!SStrCmpI(name, "OnMovieShowSubtitle", STORM_MAX_STR)) {
        data.m_wrapper = "return function(self,text) %s end";
        return &this->m_onMovieShowSubtitle;
    }

    if (!SStrCmpI(name, "OnMovieHideSubtitle", STORM_MAX_STR)) {
        return &this->m_onMovieHideSubtitle;
    }

    return nullptr;
}

void CSimpleMovieFrame::GetScriptTime(lua_State* L, double* time, uint32_t* count, int32_t recurse) {
    FrameScript_Object::GetScriptTime(L, this->m_onMovieFinished, time, count);
    FrameScript_Object::GetScriptTime(L, this->m_onMovieShowSubtitle, time, count);
    FrameScript_Object::GetScriptTime(L, this->m_onMovieHideSubtitle, time, count);
    CSimpleFrame::GetScriptTime(L, time, count, recurse);
}

const char* CSimpleMovieFrame::GetObjectTypeName() {
    return CSimpleMovieFrame::s_objectTypeName;
}

int32_t CSimpleMovieFrame::GetScriptMetaTable() {
    return CSimpleMovieFrame::s_metatable;
}

bool CSimpleMovieFrame::IsA(int32_t type) {
    return type == CSimpleMovieFrame::s_objectType || type == CSimpleFrame::s_objectType || type == CScriptRegion::s_objectType || type == CScriptObject::s_objectType;
}

bool CSimpleMovieFrame::IsA(const char* typeName) {
    return !SStrCmpI(typeName, CSimpleMovieFrame::s_objectTypeName, STORM_MAX_STR) || CSimpleFrame::IsA(typeName);
}
// CSimpleMovieFrame constructor 0x0095E6E0. Initialize the remaining inactive
// scalar fields as well; native pooled storage does not imply zero-filled bytes.
CSimpleMovieFrame::CSimpleMovieFrame(CSimpleFrame* parent) : CSimpleFrame(parent),
    m_isPlaying(0), m_finished(0), m_subtitlesEnabled(0), m_audioRestart(0),
    m_volume(100), m_decoder(0), m_fps(0), m_width(0), m_height(0), m_frameCount(0),
    m_layout(0), m_frameBuffer(nullptr), m_videoData(nullptr), m_videoPos(nullptr),
    m_videoDataSize(0), m_audioData(nullptr), m_audioDataSize(0), m_startTime(0),
    m_currentFrame(0), m_targetFrame(0), m_droppedFrames(0), m_frameOffset(0),
    m_elapsedMs(0), m_currentCaption(0) {
    m_filename[0] = 0;
    for (unsigned i = 0; i < 6; ++i) { m_textures[i] = nullptr; m_textureData[i] = {}; }
}
CSimpleMovieFrame::~CSimpleMovieFrame() {
    // Backend integration must supply real retirement before it can create any
    // of these resources. Inactive construction never owns audio/video/captions.
    if (m_isPlaying || m_decoder || m_frameBuffer || m_videoData || m_audioData || m_captions.Count()) std::terminate();
    for (auto* texture : m_textures) if (texture) std::terminate();
    if (s_currentMovieFrame == this) s_currentMovieFrame = nullptr;
}
int32_t CSimpleMovieFrame::StartMovie(const char*, int32_t) {
    Frame::Native::RequireStateMutation();
    std::cout << "native-unavailable=movie-playback" << std::endl;
    return 0; // Original script pushes nil on failed start; no finished callback.
}
void CSimpleMovieFrame::StopMovie() {
    Frame::Native::RequireStateMutation();
    if (m_isPlaying) throw std::logic_error("Native movie playback has no installed backend");
}
void CSimpleMovieFrame::OnFrameRender(CRenderBatch* batch, uint32_t layer) {
    this->CSimpleFrame::OnFrameRender(batch, layer);
    if (m_isPlaying) throw std::logic_error("Native movie rendering has no installed backend");
}
