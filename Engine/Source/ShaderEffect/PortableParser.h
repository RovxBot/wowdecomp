// Bounded native adaptation of the original WFX grammar. No game data.
// Keep the original public parser ABI and Windows comparison implementation.
#include <stdexcept>
#include <string>
#include <vector>

namespace {
class NativeEffectParser {
    using Effect = CShaderEffectParser::ParsedEffect;
    const char* data;
    size_t size;
    size_t pos = 0;
    [[noreturn]] void Fail() const {
        throw std::runtime_error("Invalid original shader effect at byte " + std::to_string(pos));
    }
    void Space() {
        for (;;) {
            while (pos < size && (data[pos]==' ' || data[pos]=='\t' || data[pos]=='\r' || data[pos]=='\n')) ++pos;
            if (pos+1 >= size || data[pos]!='/') return;
            if (data[pos+1]=='/') {
                pos+=2;
                while (pos<size && data[pos]!='\n') ++pos;
            } else if (data[pos+1]=='*') {
                pos+=2;
                while (pos+1<size && !(data[pos]=='*' && data[pos+1]=='/')) ++pos;
                if (pos+1>=size) Fail();
                pos+=2;
            } else return;
        }
    }
    bool Take(char c) {
        Space();
        if (pos<size && data[pos]==c) { ++pos; return true; }
        return false;
    }
    void Need(char c) { if (!Take(c)) Fail(); }
    std::string Word() {
        Space();
        const auto start=pos;
        while (pos<size && ((data[pos]>='a' && data[pos]<='z') ||
            (data[pos]>='A' && data[pos]<='Z') || (data[pos]>='0' && data[pos]<='9') || data[pos]=='_')) ++pos;
        if (pos==start || pos-start>=64) Fail();
        return std::string(data+start,pos-start);
    }
    std::string Call(const char* name, bool argument) {
        if (Word()!=name) Fail();
        Need('(');
        auto value=argument ? Word() : std::string();
        Need(')');
        return value;
    }
    EGxTexOp Op(const std::string& name) {
        for (unsigned i=0;i<GxTexOps_Last;++i)
            if (!SStrCmpI(name.c_str(),CShaderEffectParser::s_gxTexOpNames[i],STORM_MAX_STR))
                return CShaderEffectParser::s_gxTexOpValues[i];
        Fail();
    }
    void Fixed(Effect& effect) {
        if (effect.m_fixedFuncPassCount) Fail();
        Need('('); Need(')'); Need('{');
        const auto stages=Call("Pass",true);
        // InitFixedFuncPass stores at most two stages, independent of device caps.
        if (stages!="1" && stages!="2") Fail();
        auto& pass=effect.m_fixedFuncPasses[0];
        pass.m_pass=stages[0]-'0';
        for (unsigned i=0;i<3;++i) pass.m_colorOps[i]=pass.m_alphaOps[i]=GxTexOp_Mod;
        Need('{');
        unsigned seen=0;
        while (!Take('}')) {
            const auto name=Word();
            unsigned field;
            if (name=="ColorOp0") field=0;
            else if (name=="AlphaOp0") field=1;
            else if (name=="ColorOp1") field=2;
            else if (name=="AlphaOp1") field=3;
            else { Fail(); }
            if ((seen & (1u<<field)) || field/2>=pass.m_pass) Fail();
            seen|=1u<<field;
            Need('('); const auto op=Op(Word()); Need(')'); Need(';');
            (field%2 ? pass.m_alphaOps : pass.m_colorOps)[field/2]=op;
        }
        Need('}');
        effect.m_fixedFuncPassCount=1;
    }
    void Shader(Effect& effect) {
        if (effect.m_shaderPassCount) Fail();
        Need('('); Need(')'); Need('{'); Call("Pass",false); Need('{');
        unsigned seen=0;
        while (!Take('}')) {
            const auto name=Word();
            unsigned field;
            if (name=="VertexShader") field=1;
            else if (name=="PixelShader") field=2;
            else { Fail(); }
            if (seen & field) Fail();
            seen|=field;
            Need('('); const auto value=Word(); Need(')'); Need(';');
            auto* target=field==1 ? effect.m_shaderPasses[0].m_vertexShader : effect.m_shaderPasses[0].m_pixelShader;
            std::memcpy(target,value.c_str(),value.size()+1);
        }
        if (seen!=3) Fail();
        Need('}'); effect.m_shaderPassCount=1;
    }
public:
    NativeEffectParser(const char* bytes, size_t length) : data(bytes),size(length) {
        if (!bytes || !length || length>1024*1024 || std::memchr(bytes,0,length)) Fail();
    }
    std::vector<Effect> Parse() {
        std::vector<Effect> effects;
        for (;;) {
            Space(); if (pos==size) break;
            if (effects.size()>=256) Fail();
            Effect effect{};
            const auto name=Call("Effect",true);
            std::memcpy(effect.m_name,name.c_str(),name.size()+1);
            for (const auto& prior:effects)
                if (!SStrCmpI(prior.m_name,effect.m_name,STORM_MAX_STR)) Fail();
            Need('{');
            while (!Take('}')) {
                const auto section=Word();
                if (section=="FixedFunc") Fixed(effect);
                else if (section=="Shader") Shader(effect);
                else Fail();
            }
            if (!effect.m_fixedFuncPassCount && !effect.m_shaderPassCount) Fail();
            effects.push_back(effect);
        }
        if (effects.empty()) Fail();
        return effects;
    }
};
}
