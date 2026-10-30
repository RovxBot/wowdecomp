#include <cstddef>
#include <cstdint>
#include <cstdio>
#include "Engine/Source/ShaderEffect/ShaderEffectManager.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/Storm/H/storm.h"
#ifdef WOWDECOMP_PORTABLE_RUNTIME
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#endif

// STATIC: 0x00D43408
TSHashTable<CShaderEffect, HASHKEY_STRI> CShaderEffectManager::m_ShaderList;

// VERIFY: 554f80ac manual
// OFFSET: 0x008764D0
void CShaderEffectManager::DestroyAllEffects() {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    CShaderEffect::s_curEffect = nullptr;
#endif
    CShaderEffect* effect = CShaderEffectManager::m_ShaderList.m_fulllist.Head();

    while (effect) {
        effect = CShaderEffectManager::m_ShaderList.DeleteNode(effect);
    }
}

// VERIFY: e6522edb auto
// OFFSET: 0x00876530
CShaderEffect* CShaderEffectManager::GetEffect(const char* name) {
    return CShaderEffectManager::m_ShaderList.Ptr(name);
}

// VERIFY: 82eb3c99 manual
// OFFSET: 0x00876BE0
CShaderEffect* CShaderEffectManager::CreateEffect(const char* name) {
    CShaderEffectManager::m_ShaderList.Ptr(name);

    return CShaderEffectManager::m_ShaderList.New(name, 0, 0);
}

// Original Windows verification; native validates before publication and rolls back.
// VERIFY: 156a1818 manual
// OFFSET: 0x00876CA0
void CShaderEffectManager::ParseEffectCallback(const CShaderEffectParser::ParsedEffect& effect, void* param) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    if (GetEffect(effect.m_name)) throw std::runtime_error("Duplicate original shader effect");
#endif
    CShaderEffect* shaderEffect = CShaderEffectManager::m_ShaderList.New(effect.m_name, 0, 0);

#ifdef WOWDECOMP_PORTABLE_RUNTIME
    try {
        shaderEffect->InitEffect(effect.m_shaderPassCount ? effect.m_shaderPasses[0].m_vertexShader : nullptr,
            effect.m_shaderPassCount ? effect.m_shaderPasses[0].m_pixelShader : nullptr);
#else
    shaderEffect->InitEffect(effect.m_shaderPasses[0].m_vertexShader, effect.m_shaderPasses[0].m_pixelShader);
#endif

    for (uint32_t i = 0; i < effect.m_fixedFuncPassCount; i++) {
        const CShaderEffectParser::ParsedEffect::FixedFuncPass& pass = effect.m_fixedFuncPasses[i];
        shaderEffect->InitFixedFuncPass(pass.m_colorOps, pass.m_alphaOps, pass.m_pass);
    }
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    } catch (...) { m_ShaderList.DeleteNode(shaderEffect); throw; }
#endif
}

// Original Windows verification; native retains the authenticated file and checks full reads.
// VERIFY: c6a3a3f5 manual
// OFFSET: 0x00876D90
void CShaderEffectManager::AddEffectFile(const char* fileName) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    if (!fileName || !*fileName || std::strlen(fileName)>128 || std::strpbrk(fileName,"/\\:"))
        throw std::invalid_argument("Invalid original effect basename");
    const std::string path=std::string("Shaders\\Effects\\")+fileName;
    SFileImpl* file=nullptr;
    if (!SFileOpenFile(path.c_str(),&file) || !file)
        throw std::runtime_error("Missing original shader effect file: "+path);
    struct Close { SFileImpl* file; ~Close() { SFileCloseFile(file); } } close{file};
    uint32_t high=0;
    const auto size=SFileGetFileSize(file,&high);
    if (high || !size || size>1024*1024) throw std::runtime_error("Invalid original shader effect size: "+path);
    std::vector<char> bytes(size);
    uint32_t read=0;
    if (!SFileReadFile(file,bytes.data(),size,&read,nullptr,0) || read!=size)
        throw std::runtime_error("Incomplete original shader effect read: "+path);
    std::vector<CShaderEffectParser::ParsedEffect> effects;
    CShaderEffectParser::ParseEffectFile(bytes.data(),size,[](const auto& effect,void* param) {
        static_cast<decltype(effects)*>(param)->push_back(effect);
    },&effects);
    for (const auto& effect:effects) {
        if (GetEffect(effect.m_name) || (!CShaderEffect::s_enableShaders && !effect.m_fixedFuncPassCount))
            throw std::runtime_error("Unavailable or duplicate original shader effect: "+std::string(effect.m_name));
    }
    size_t installed=0;
    try {
        for (const auto& effect:effects) { ParseEffectCallback(effect,nullptr); ++installed; }
    } catch (...) {
        for (size_t i=0;i<installed;++i) m_ShaderList.DeleteNode(GetEffect(effects[i].m_name));
        throw;
    }
#else
    char path[256];
    sprintf(path, "Shaders\\Effects\\%s", fileName);

    SFileImpl* file;
    SFileOpenFile(path, &file);

    if (!file) {
        return;
    }

    uint32_t size = SFileGetFileSize(file, nullptr);
    char* data = static_cast<char*>(SMemAlloc(size + 1, __FILE__, __LINE__, 0));
    SFileReadFile(file, data, size, nullptr, nullptr, 0);
    SFileCloseFile(file);

    CShaderEffectParser::ParseEffectFile(data, size, CShaderEffectManager::ParseEffectCallback, nullptr);

    delete[] data;
#endif
}
