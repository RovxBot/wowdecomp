// Authored WFX and particle geometry regressions; contains no game data.
#include "Engine/Source/ShaderEffect/ShaderEffectManager.h"
#include "Engine/Source/Services/Texture.h"
namespace ParticleChecks {
inline std::string Definition(const std::string& name,unsigned stages=1) {
    return "Effect("+name+"){FixedFunc(){Pass("+std::to_string(stages)+
        "){ColorOp0(Mod);AlphaOp0(Mod);}}Shader(){Pass(){VertexShader(NativeVertex);PixelShader(NativePixel);}}}";
}
inline std::string Models() {
    std::string text;
    for (const auto* name:{"Projected_ModMod","Projected_ModAdd","Projected_ModMod_Unlit","Projected_ModAdd_Unlit"})
        text+=Definition(name,2);
    return text;
}
inline std::string Particles() { return Definition("Particle")+Definition("Particle_Unlit"); }
inline void Parsing() {
    const auto valid=Definition("NativeSyntax");
    auto parse=[](std::string text,unsigned& calls) {
        CShaderEffectParser::ParseEffectFile(text.data(),text.size(),[](const auto&,void* p) {
            ++*static_cast<unsigned*>(p);
        },&calls);
    };
    unsigned calls=0;
    parse("// heading\n/* bounded comment */"+valid,calls);
    Check(calls==1,"Authored WFX did not parse");
    for (size_t length=0;length<valid.size();++length) {
        calls=0; bool rejected=false;
        try { parse(valid.substr(0,length),calls); } catch (const std::runtime_error&) { rejected=true; }
        Check(rejected && !calls,"Truncated WFX published a partial effect");
    }
    std::vector<std::string> invalid={
        valid+"Effect(Broken){", valid+Definition("nativesyntax"),
        "Effect("+std::string(64,'x')+"){FixedFunc(){Pass(1){}}}",
        "Effect(A,B){FixedFunc(){Pass(1){}}}",
        "Effect(A){FixedFunc(){Pass(3){}}}",
        "Effect(A){FixedFunc(){Pass(-1){}}}",
        "Effect(A){FixedFunc(){Pass(1){}Pass(1){}}}",
        "Effect(A){FixedFunc(){Pass(1){ColorOp0(Unknown);}}}",
        "Effect(A){FixedFunc(){Pass(1){ColorOp1(Mod);}}}",
        "Effect(A){Shader(){Pass(){VertexShader(A);}}}",
        "Effect(A){Shader(){Pass(){VertexShader(A);PixelShader(B);}Pass(){}}}",
        "Effect(A){FixedFunc(){Pass(1){ColorOp0(Mod);ColorOp0(Add);}}}",
        "Effect(A){}",valid+"/*",valid+"}",std::string(1024*1024+1,' ')};
    auto embedded=valid; embedded[3]=0; invalid.push_back(embedded);
    for (const auto& text:invalid) {
        calls=0; bool rejected=false;
        try { parse(text,calls); } catch (const std::runtime_error&) { rejected=true; }
        Check(rejected && !calls,"Malformed WFX escaped bounded transactional parsing");
    }
    std::cout<<"native-effect-bounded-parser=passed\n";
}
inline void Rendering(CM2Scene* scene,Gx::Native::LegacyDevice& device) {
    CM2Shared shared(scene->m_cache);
    shared.m_instances=1; shared.m_refCount=2;
    CM2Model model; model.m_shared=&shared;
    CM2Lighting lighting{};
    lighting.m_flags|=2; lighting.m_fogScale=0; lighting.m_lightCount=0;
    lighting.m_sunAmbient=C3Vector(1,1,1); lighting.m_sunDir=C3Vector(0,0,-1);
    model.m_currentLighting=&lighting;
    device.RsPush();
    struct Restore { Gx::Native::LegacyDevice& d; ~Restore() { d.RsPop(); } } restore{device};
    GxRsSet(GxRs_VertexShader,static_cast<void*>(nullptr));
    GxRsSet(GxRs_PixelShader,static_cast<void*>(nullptr));
    GxXformSetProjection(C44Matrix()); GxXformSetView(C44Matrix());
    GxXformSetViewport(0,1,0,1,0,1); device.m_xforms[GxXform_World].Identity();
    for (unsigned lit=0;lit<2;++lit) {
        CPlaneParticleEmitter emitter;
        emitter.SetParticleStyle(1,0,1,0);
        emitter.m_flags|=0x1000000; // Original simple lifetime/color/scale keys.
        emitter.m_params.m_materialFlags=lit;
        emitter.m_params.m_materialBlend=2; // Original alpha blend.
        emitter.m_params.m_life=1; emitter.m_params.m_simpleMidTime=.5f;
        emitter.m_params.m_simpleKeys=new CParticleEmitter2::SimpleKey[2];
        for (unsigned i=0;i<2;++i) {
            auto& key=emitter.m_params.m_simpleKeys[i];
            key.m_color=CImVector(0,0,255,128); key.m_scale=.35f;
            key.m_deltaScale=0; key.m_headCell=key.m_tailCell=0;
            key.m_deltaHeadCell=key.m_deltaTailCell=0;
        }
        emitter.m_texture=TextureCreateSolid(CImVector(255,255,255,255));
        emitter.m_particles.SetCount(1);
        auto& particle=emitter.m_particles[0]; particle={}; particle.m_age=.25f;
        uint32_t live=0; emitter.m_liveList.New(live);
        M2Element element{};
        element.m_type=4; element.m_model=&model; element.m_emitter=&emitter; element.m_alpha=1;
        uint32_t index=0;
        for (unsigned repeat=0;repeat<3;++repeat) {
            device.SceneClear(GxBufferBit_Color|GxBufferBit_Depth,CImVector(255,0,0,255));
            CM2SceneRender render(scene);
            render.m_curType=4; render.m_curElement=&element; render.m_curModel=&model;
            render.m_curShared=&shared; render.m_curLighting=&lighting;
            render.DrawParticle(0,&element,&index,1);
            Check(emitter.m_renderedParticles==1 && device.m_primVertexFormat==(lit ? GxVBF_PNCT : GxVBF_PCT),
                "Original particle geometry/vertex format was not submitted");
            CiRect rect; rect.minX=160; rect.maxX=161; rect.minY=120; rect.maxY=121;
            TSGrowableArray<CImVector> pixels; device.DeviceReadPixels(rect,pixels);
            Check(pixels.Count()==1 && pixels[0].r>=126 && pixels[0].r<=130 &&
                pixels[0].g<=2 && pixels[0].b>=125 && pixels[0].b<=129,
                ("Original lit/unlit particle alpha pixel: "+std::to_string(pixels[0].r)+","+
                std::to_string(pixels[0].g)+","+std::to_string(pixels[0].b)).c_str());
        }
        emitter.StepUpdate(2,1);
        Check(!emitter.m_liveList.Count(),"Original particle lifetime did not retire the live particle");
    }
    CShaderEffect::s_curEffect=nullptr;
    std::cout<<"native-original-particle-lit-unlit-geometry-alpha-pixels-lifetimes=passed\n";
}
}
