// Original 12340 CGlueMgr::Resume interface gate (0x004DA5F0), adapted to the
// qualified owner-thread native FrameScript/XML/services ABI. No new VERIFY claim.
#include "Startup.h"
#include "WoW/Source/Glue/Types.h"
#include "Engine/Source/FrameXML/Portable/Runtime.h"
#include "Engine/Source/FrameXML/FrameXML.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Frame/Portable/Runtime.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/Storm/Source/MD5.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace GlueNative {
void LoadInterface() {
    Frame::Native::LoadScriptEdits loadEdits;
    const char* toc = "Interface\\GlueXML\\GlueXML.toc";
    uint8_t expected[16], actual[16];
    if (FrameXML_CheckSignature(toc, nullptr, FrameXML::Native::OriginalInterfaceKey(), expected) != 3)
        throw std::runtime_error("Original GlueXML InterfaceKey authentication failed");
    std::cout << "startup-stage=glue-authenticated visits=" << FrameXML::Native::LastTraversal().files << std::endl;
    MD5_CTX digest; MD5Init(&digest);
    CStatus status;
    if (!FrameXML_CreateFrames(toc, nullptr, &digest, &status))
        throw std::runtime_error("Original ordered GlueXML creation failed");
    MD5Final(actual, &digest);
    if (std::memcmp(actual, expected, sizeof(actual)))
        throw std::runtime_error("Original post-creation GlueXML digest mismatch");
    if (status.m_highestSeverity >= STATUS_ERROR) {
        char error[4096]; throw std::runtime_error(status.GetErrorStr(error, sizeof(error), STATUS_ERROR, "; "));
    }
    std::cout << "startup-stage=glue-created post-creation-digest=matched" << std::endl;
    const auto errors = FrameScript::Native::ScriptErrorCount();
    FrameScript_SignalEvent(GLUE_EVENT_FRAMES_LOADED, nullptr);
    if (FrameScript::Native::ScriptErrorCount() != errors)
        throw std::runtime_error("Original Glue FRAMES_LOADED failed: " + FrameScript::Native::LastScriptError());
    // Original CGlueMgr::SetScreen (0x004D82A0): let unchanged GlueParent choose
    // and show the login frame. Missing activation services remain real errors.
    FrameScript_SignalEvent(GLUE_EVENT_SET_GLUE_SCREEN, "%s", "login");
    if (FrameScript::Native::ScriptErrorCount() != errors)
        throw std::runtime_error("Original Glue screen activation failed: " + FrameScript::Native::LastScriptError());
}
}
