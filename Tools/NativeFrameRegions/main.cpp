#include "Engine/Source/Frame/CSimpleRender.h"
#include "Engine/Source/Frame/CRenderBatch.h"
#include "Engine/Source/Frame/Portable/Render.h"
#include <limits>
#include "Engine/Source/Frame/Portable/Fonts.h"
#include "Engine/Source/Frame/CScriptObject.h"
#include "Engine/Source/Frame/CSimpleFont.h"
#include "Engine/Source/FrameXML/XMLTree.h"
#include "Engine/Source/FrameXML/LoadXML.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Gx/Portable/LegacyDevice.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Gxu/GxuFont.h"
#include "Engine/Source/Gxu/GxuFontMiscClasses.h"
#include "Engine/Source/Services/TextBlock.h"
#include "Engine/Source/Services/Portable/AsyncFile.h"
#include "Engine/Source/BlizzardCore/Portable/Assets.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Os/Portable/Runtime.h"
#include "Engine/Source/Os/Time.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include <algorithm>
#include <cfenv>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <thread>
using Bytes = std::vector<uint8_t>;
using namespace Gx::Native;
using namespace Services::Native;
static void Check(bool value, const char* why) { if (!value) throw std::runtime_error(why); }
template<class F> static void Reject(F f) {
    bool rejected=false; try { f(); } catch (const std::logic_error&) { rejected=true; }
    Check(rejected,"Invalid Frame font operation accepted");
}
static Bytes Program(const char* program, uint32_t count=1) {
    Bytes bytes;
    auto word=[&](uint32_t value) { for (int i=0;i<4;++i) bytes.push_back(uint8_t(value>>(8*i))); };
    word(0x47585348);word(0x10003);word(0);
    for (uint32_t i=0;i<count;++i) {
        word(0);word(0);word(0);word(uint32_t(std::strlen(program)+1));
        bytes.insert(bytes.end(),program,program+std::strlen(program)+1);
        while(bytes.size()%4)bytes.push_back(0);
    }
    return bytes;
}
struct Source {
    FileQueue queue;
    std::shared_ptr<Blizzard::Mopaq::Native::Packages> packages;
    Bytes fixture;
    std::map<std::string,Bytes> shaders;
    Source(const std::string& data,const std::string& settings,const std::string& cache,const std::string& font) {
        if (data.empty()) {
            std::ifstream file(font,std::ios::binary);
            fixture.assign(std::istreambuf_iterator<char>(file),{});
            Check(!fixture.empty(),"Missing authored font");
            shaders["Shaders\\Vertex\\arbvp1\\UI.bls"]=Program(
                "!!ARBvp1.0\nDP4 result.position.x, program.env[0], vertex.position;\nDP4 result.position.y, program.env[1], vertex.position;\nDP4 result.position.z, program.env[2], vertex.position;\nDP4 result.position.w, program.env[3], vertex.position;\nMOV result.color, vertex.attrib[4];\nMOV result.texcoord[0], vertex.attrib[6];\nEND\n",2);
            shaders["Shaders\\Pixel\\arbfp1\\UI.bls"]=Program(
                "!!ARBfp1.0\nTEMP tex;\nTEX tex, fragment.texcoord[0], texture[0], 2D;\nMUL result.color,tex,fragment.color;\nEND\n");
        } else {
            Blizzard::File::Native::Paths paths;
            Check(Blizzard::File::Native::ConfigurePaths(data.c_str(),settings.c_str(),cache.c_str(),paths),"Separate data/user roots required");
            auto ticket=queue.Submit([&](const Cancellation&) {
                packages=Blizzard::Mopaq::Native::Packages::Open12340(paths,"enUS");return ReadResult{};
            },[](const ReadResult& result) { Check(result.status==ReadStatus::Success,"Original package authentication failed"); });
            if (!queue.Wait(ticket,std::chrono::seconds(60))) {
                queue.Shutdown();throw std::runtime_error("Package open timeout");
            }
            for (const char* path : {"Shaders\\Vertex\\arbvp1\\UI.bls","Shaders\\Pixel\\arbfp1\\UI.bls"}) {
                Bytes bytes;Check(Read(path,bytes),"Original shader read failed");shaders[path]=std::move(bytes);
            }
            std::cout << "original-signaturefile=authenticated records=" << packages->SignatureCount() << '\n';
        }
    }
    bool Read(const std::string& path,Bytes& bytes) {
        if (!packages) return false;
        auto result=std::make_shared<ReadResult>();
        auto ticket=queue.ReadPackage(packages,path,[result](const ReadResult& value){*result=value;});
        if (!queue.Wait(ticket,std::chrono::seconds(60))) {queue.Cancel(ticket);return false;}
        bytes=std::move(result->file.bytes);return result->status==ReadStatus::Success;
    }
    Bytes Font(const std::string& path) {
        if (!packages) return path=="bad.ttf" ? Bytes{0,1,0,0,0,0,0,0} : fixture;
        Bytes bytes;Check(Read(path,bytes),"Original font read failed");return bytes;
    }
};
static CSimpleFont* Original(Source& source) {
    uint8_t before[16],after[16];
    Check(FrameXML_CheckSignature("Interface\\GlueXML\\GlueXML.toc",nullptr,FrameXML::Native::OriginalInterfaceKey(),before)==3,"Original GlueXML interface signature");
    CStatus status;
    std::unique_ptr<XMLTree,decltype(&XMLTree_Free)> tree(FrameXML_LoadXML("Interface\\GlueXML\\GlueFonts.xml",nullptr,&status),&XMLTree_Free);
    Check(bool(tree),"Original GlueFonts XML read");
    size_t count=0;CSimpleFont* selected=nullptr;
    for (auto* node=XMLTree_GetRoot(tree.get())->m_child;node;node=node->m_next) {
        Check(!SStrCmpI(node->m_name.m_str,"Font",STORM_MAX_STR),"Unsupported original font definition tag");
        auto* font=CSimpleFont::GetFont(node->GetAttributeByName("name"),1);font->LoadXML(node,&status);
        Check(font->m_attributes.m_flags&0x100,"Original font definition did not resolve a font");
        if (!selected) selected=font;
        ++count;
    }
    tree.reset();
    if(!status.IsEmpty()){char error[4096];throw std::runtime_error(status.GetErrorStr(error,sizeof(error),STATUS_WARNING,"; "));}
    Check(selected && count>10,"Original GlueFonts factory count");
    Check(FrameXML_CheckSignature("Interface\\GlueXML\\GlueXML.toc",nullptr,FrameXML::Native::OriginalInterfaceKey(),after)==3 && !std::memcmp(before,after,16),"Original content changed during font construction");
    std::cout<<"original-GlueFonts-factories="<<count<<" interface-signature=authenticated\n";
    return selected;
}

struct LayoutRoot final : CLayoutFrame {
    LayoutRoot() {
        m_rect.minX=m_rect.minY=0;m_rect.maxX=0.8f;m_rect.maxY=0.6f;
        m_layoutFlags|=0x100;
    }
};
static std::vector<CImVector> Pixels(LegacyDevice& device) {
    CiRect rect{};rect.maxX=640;rect.maxY=480;TSGrowableArray<CImVector> pixels;
    device.DeviceReadPixels(rect,pixels);return {pixels.Ptr(),pixels.Ptr()+pixels.Count()};
}
static std::vector<CImVector> Draw(LegacyDevice& device,CRenderBatch& batch) {
    GxSceneClear(GxBufferBit_Color,CImVector(0,0,0,255));
    batch.Finish();CSimpleRender::DrawBatch(&batch);return Pixels(device);
}
static void Equal(const std::vector<CImVector>& a,const std::vector<CImVector>& b) {
    Check(a.size()==b.size() && !std::memcmp(a.data(),b.data(),a.size()*sizeof(CImVector)),"Region repeat pixels changed");
}
struct CallbackProbe {
    CSimpleFontString& region;
    static void Run(void* p) {
        auto& r=static_cast<CallbackProbe*>(p)->region;
        Reject([&]{r.SetText("callback",0);});
        Reject([&]{r.SetFontObject(nullptr);});
        Reject([&]{r.SetVertexAlpha(1);});
        Reject([&]{r.SetPoint(FRAMEPOINT_CENTER,&r,FRAMEPOINT_CENTER,0,0,1);});
    }
};
static void Regions(LegacyDevice& device,CSimpleFont& font,Frame::Native::Fonts& factories,const std::string& snapshot) {
    LayoutRoot root;
    auto text=std::make_unique<CSimpleFontString>(nullptr,2,true);
    Check(text->m_maxLines==0x1000000 && text->m_fontHeight==0 && text->m_shadowOffset.x==0 && text->m_shadowOffset.y==0 && text->m_drawLayer==2 && text->m_shown,"Region native initialization");
    Check(!text->m_parent && !text->m_visible && !text->HasActiveAnimGroup(),"Unparented region state");
    text->SetSize(0.6f,0.3f);
    text->SetPoint(FRAMEPOINT_BOTTOMLEFT,&root,FRAMEPOINT_BOTTOMLEFT,0.1f,0.1f,1);
    text->SetFontObject(&font);
    text->SetText("AV Frame region",0);
    text->SetSpacing(NDCToDDCHeight(1.0f/480));
    CLayoutFrame::ResizePending();
    Check(text->m_fontHandle && text->m_string && text->GetStringWidth()>0 && text->GetStringHeight()>0,"Original FontString layout did not create text");
    Reject([&]{factories.CheckCanClose();});
    bool foreign=false;std::thread thread([&]{try{text->SetText("thread",0);}catch(const std::logic_error&){foreign=true;}});thread.join();Check(foreign,"Foreign region access accepted");
    Reject([&]{text->SetFrame(nullptr,5,1);});
    Reject([&]{text->SetParent(reinterpret_cast<CSimpleFrame*>(uintptr_t(1)));});
    Reject([&]{text->RegisterAnimGroup(nullptr);});
    Reject([&]{text->GetScriptMetaTable();});
    const auto stack=lua_gettop(s_context), expected=lua_taintexpected;
    const auto taint=lua_tainted;
    Reject([&]{text->RegisterScriptObject(nullptr);});
    Reject([&]{text->SetName("UnavailableRegion");});
    Check(lua_gettop(s_context)==stack && lua_taintexpected==expected && lua_tainted==taint && !text->GetName(),"Unavailable region script factory changed VM/name state");
    Reject([&]{text->SetText("processed",1);});
    Reject([&]{text->SetText("|Tunsupported:16|t",0);});
    Reject([&]{text->SetTextLength(65535);});
    Reject([&]{text->SetFont("bad.ttf",std::numeric_limits<float>::infinity(),0,false);});
    Reject([&]{text->SetSpacing(std::numeric_limits<float>::quiet_NaN());});
    Reject([&]{text->SetAlphaGradient(40000,1);});
    {CRenderBatch batch;CallbackProbe callback{*text};text->Draw(&batch);batch.QueueCallback(&CallbackProbe::Run,&callback);Draw(device,batch);}
    // Screen callbacks are direct fixture injection, preceding actual SDL/root delivery.
    GxuFontWindowSizeChanged();text->OnScreenSizeChanged();CLayoutFrame::ResizePending();
    Check(text->m_string,"Screen invalidation lost the original text block");
    text->SetLayoutScale(1.25f,true);CLayoutFrame::ResizePending();
    Check(text->m_string,"Scale change lost the original text block");
    // Font replacement must close the prior block before its borrowed font.
    const std::string face=text->GetFontName();const auto height=text->m_fontHeight;
    Check(text->SetFont(face.c_str(),height,text->GetFontFlags(),true),"Font replacement failed");CLayoutFrame::ResizePending();
    text->AddShadow(CImVector(0,0,0,255),C2Vector(0.001f,-0.001f));
    text->SetShadowOffset(C2Vector(0.002f,-0.002f));text->RemoveShadow();
    text->SetVertexColor(CImVector(0,0,255,255));
    CRenderBatch retained;
    text->Draw(&retained);auto first=Draw(device,retained);
    size_t ink=0;for(const auto& p:first)ink+=p.r>16 && p.g<4 && p.b<4;
    Check(ink>4,"Actual FontString region rendered no red glyphs");
    auto oldBlock=text->m_string;auto oldFont=text->m_fontHandle;
    text->SetText("replacement",0);CLayoutFrame::ResizePending();
    Check(text->m_string && text->m_string!=oldBlock,"FontString did not replace its text block");
    // The retained batch still holds the old block AND font; the consumer can close.
    text.reset();Equal(first,Draw(device,retained));
    Check(TextBlockGetStringPtr(oldBlock) && TextBlockGetFontName(oldFont),"Queued FontString resources did not survive destruction");
    retained.Clear();
    if(!snapshot.empty()) {
        std::ofstream out(snapshot,std::ios::binary);out<<"P6\n640 480\n255\n";
        for(int y=479;y>=0;--y)for(int x=0;x<640;++x){const auto& p=first[y*640+x];char rgb[]={char(p.r),char(p.g),char(p.b)};out.write(rgb,3);}
        Check(bool(out),"Region snapshot failed");
    }
    Check(!root.m_resizeList.Head(),"Closed FontString left a layout dependency");
    factories.CheckCanClose();
    std::cout<<"frame-regions-layout-font-inheritance-replacement-batch-retention=passed red-ink="<<ink<<'\n';
}
int main(int argc,char** argv) {
    bool started=false;
    try {
        std::string data,settings,cache,font,snapshot;
        for(int i=1;i<argc;++i){Check(i+1<argc,"Missing option value");std::string key=argv[i],value=argv[++i];
            if(key=="--data")data=value;else if(key=="--settings")settings=value;else if(key=="--cache")cache=value;else if(key=="--font")font=value;else if(key=="--snapshot")snapshot=value;else throw std::invalid_argument("Unknown region option");}
        Check(data.empty()==settings.empty() && data.empty()==cache.empty(),"Separate original data/user roots required");Check(!data.empty() || !font.empty(),"Supply authored font or original data");
        Check(!std::fesetenv(FE_DFL_ENV) && !std::fesetround(FE_TONEAREST),"FP initialization failed");
        StormRtlInitialize();OsTimeStartup(OsTimeManager::BestAvailable);started=true;
        for(int cycle=0;cycle<3;++cycle){
            {
                GlContext context("Original FontString region fixture",640,480,true);
                std::cout<<"gl-renderer="<<context.Info().renderer<<"\ngl-version="<<context.Info().version<<'\n';
                Source source(data,settings,cache,font);
                LegacyDevice device(context,[&](const std::string& path){return source.shaders.at(path);});
                GxRsSet(GxRs_Lighting,0);GxRsSet(GxRs_Fog,0);GxRsSet(GxRs_Culling,0);
                FrameScript::Native::Runtime scripts([&](FrameScript::Native::SourceKind kind,const std::string& path,Bytes& bytes){return kind==FrameScript::Native::SourceKind::Interface && source.Read(path,bytes);});
                FrameXML::Native::Runtime xml;Frame::Native::Runtime layout;
                Gxu::Native::LegacyFonts gxu([&](const std::string& path){return source.Font(path);});
                {Frame::Native::Fonts factories;Frame::Native::Renderer renderer;
                    CSimpleFont* selected;
                    if(data.empty()) {selected=CSimpleFont::GetFont("RegionFont",1);Check(selected->m_attributes.SetFont("Fonts\\fixture.ttf",NDCToDDCHeight(32.0f/480),0),"Authored font creation");}
                    else selected=Original(source);
                    Regions(device,*selected,factories,cycle?"":snapshot);
                    renderer.CheckCanClose();factories.CheckCanClose();}
                Check(!source.queue.Pending(),"Region left pending file work");
            }
            Check(!s_context && !OsLiveThreadCount(),"Region shutdown left VM/workers");std::cout<<"frame-regions-cycle-closed="<<cycle+1<<" workers=0\n";
        }
        OsTimeShutdown();StormRtlDestroy();return 0;
    }catch(const std::exception& error){std::cerr<<"NativeFrameRegionsCheck failed: "<<error.what()<<'\n';if(started){OsTimeShutdown();StormRtlDestroy();}return 1;}
}
