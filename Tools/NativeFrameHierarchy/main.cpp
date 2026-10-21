#include "Engine/Source/Frame/Portable/Hierarchy.h"
#include "Engine/Source/Frame/CSimpleTop.h"
#include "Engine/Source/Frame/CFrameStrataNode.h"
#include "Engine/Source/Frame/CSimpleRender.h"
#include "Engine/Source/Frame/CRenderBatch.h"
#include "Engine/Source/Frame/Portable/Render.h"
#include "Engine/Source/Gxu/GxuFontString.h"
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
template<class F> static void RejectAt(F f,int line) {
    bool rejected=false; try { f(); } catch (const std::logic_error&) { rejected=true; }
    if (!rejected) throw std::runtime_error("Invalid Frame operation accepted at fixture line " + std::to_string(line));
}
#define Reject(...) RejectAt(__VA_ARGS__, __LINE__)
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

static std::vector<CImVector> Draw(LegacyDevice& device, CSimpleTop& root) {
    root.OnLayerUpdate(0.016f);
    GxSceneClear(GxBufferBit_Color,CImVector(0,0,0,255));
    root.OnLayerRender();
    CiRect rect{};rect.maxX=640;rect.maxY=480;TSGrowableArray<CImVector> pixels;
    device.DeviceReadPixels(rect,pixels);return {pixels.Ptr(),pixels.Ptr()+pixels.Count()};
}
static void Equal(const std::vector<CImVector>& a,const std::vector<CImVector>& b) {
    Check(a.size()==b.size() && !std::memcmp(a.data(),b.data(),a.size()*sizeof(CImVector)),"Frame root repeat pixels changed");
}
static size_t Ink(const std::vector<CImVector>& pixels) {
    return std::count_if(pixels.begin(),pixels.end(),[](const CImVector& p){return p.r>8 || p.g>8 || p.b>8;});
}
struct UpdateFrame final : CSimpleFrame {
    int& deleted;
    UpdateFrame(CSimpleFrame* parent,int& count):CSimpleFrame(parent),deleted(count) {}
    ~UpdateFrame() override { ++deleted; }
    void OnLayerUpdate(float) override {
        Reject([&]{SetParent(nullptr);});
        Reject([&]{Frame::Native::RequireTreeMutation();});
        Frame::Native::DestroyLater(this);
        Frame::Native::DestroyLater(this);
    }
};
struct GuardedText final : CSimpleFontString {
    explicit GuardedText(CSimpleFrame* parent):CSimpleFontString(parent,2,true) {}
    void Draw(CRenderBatch* batch) override {
        Reject([&]{SetFrame(m_parent,0,1);});
        Reject([&]{Hide();});
        Reject([&]{m_parent->SetParent(nullptr);});
        CSimpleFontString::Draw(batch);
    }
};
static void Hierarchy(LegacyDevice& device, CSimpleFont& font,const std::string& snapshot) {
    CSimpleTop root;
    Check(root.m_rect.maxX>0 && root.m_rect.maxY>0,"Original root extent missing");
    Reject([&]{CSimpleTop second;});
    auto* parent=new CSimpleFrame(nullptr);
    auto* child=new CSimpleFrame(parent);
    Check(child->m_parent==parent && parent->m_children.Head()->m_frame==child && child->m_isVisible,"Original Frame child registration");
    parent->SetSize(0.7f,0.5f);
    parent->SetPoint(FRAMEPOINT_BOTTOMLEFT,&root,FRAMEPOINT_BOTTOMLEFT,0.05f,0.05f,1);
    child->SetAllPoints(parent,1);
    auto* text=new GuardedText(child);
    text->SetAllPoints(child,1);
    text->SetFontObject(&font);text->SetText("AV Frame parent",0);
    Check(text->m_parent==child && text->m_visible && child->m_drawLayers[2].Head()==text,"Original parent region/layer registration");
    Check(text->GetLayoutParent()==child && child->GetLayoutParent()==parent && parent->GetLayoutParent()==&root,"Original layout parents/root");
    Check(text->GetScriptObjectParent()==child,"Original script parent borrow");
    auto first=Draw(device,root);auto ink=Ink(first);
    Check(ink>4 && text->m_string,"Root rendered no actual FontString glyphs");
    Equal(first,Draw(device,root));
    parent->AllowAttributeChanges(1);parent->AllowAttributeChanges(1);
    Check(root.m_destroyed.Head()==parent && !root.m_destroyed.Next(parent),"Attribute permission cleanup queued twice");
    root.OnLayerUpdate(0.016f);Check(!parent->AttributeChangesAllowed() && !root.m_destroyed.Head(),"Original temporary attribute permission did not expire");
    parent->Hide();Check(!child->m_isVisible && !text->m_visible && text->m_shown,"Hidden parent visibility inheritance");
    Check(!Ink(Draw(device,root)),"Hidden parent still rendered");
    parent->Show();Equal(first,Draw(device,root));
    text->Hide();Check(!Ink(Draw(device,root)),"Hidden region still rendered");
    text->SetFrame(child,2,1);Equal(first,Draw(device,root));
    child->DisableDrawLayer(2);Check(!Ink(Draw(device,root)),"Disabled draw layer still rendered");
    child->EnableDrawLayer(2);Equal(first,Draw(device,root));
    // Move the same live text through earlier/later queues without regenerating
    // its block. Every dirty source must release its intrusive batch link first.
    child->SetFrameLevel(0,1);Equal(first,Draw(device,root));
    child->SetFrameStrata(FRAME_STRATA_BACKGROUND);Equal(first,Draw(device,root));
    text->SetFrame(child,0,1);Equal(first,Draw(device,root));
    text->SetFrame(child,3,1);Equal(first,Draw(device,root));
    text->SetFrame(parent,2,1);Equal(first,Draw(device,root));
    Check(!child->m_regions.Head() && parent->m_regions.Head()==text,"Region reparent left old list membership");
    text->SetFrame(child,2,1);Equal(first,Draw(device,root));
    parent->SetFrameAlpha(128);Check(child->m_parentAlpha==128 && text->m_colors[0].a==128,"Inherited Frame alpha");
    auto faded=Draw(device,root);Check(Ink(faded)>0,"Inherited alpha lost all glyphs");
    parent->SetFrameAlpha(255);Equal(first,Draw(device,root));
    parent->SetFrameScale(1.25f,true);Draw(device,root);
    Check(child->m_layoutScale==1.25f && text->m_layoutScale==1.25f,"Inherited Frame scale");
    parent->SetFrameScale(1.0f,true);Equal(first,Draw(device,root));
    // Parent graph bounds/cycles are rejected before any original list changes.
    std::vector<CSimpleFrame*> deep{new CSimpleFrame(nullptr)};
    for (int i=1;i<64;++i) deep.push_back(new CSimpleFrame(deep.back()));
    Reject([&]{new CSimpleFrame(deep.back());});
    Reject([&]{deep.front()->SetParent(deep.back());});
    delete deep.front();
    child->SetFrameLevel(1,1);Equal(first,Draw(device,root));
    const int oldLevel=parent->m_level;
    Reject([&]{parent->SetFrameLevel(128,1);});
    Check(parent->m_level==oldLevel && child->m_parent==parent,"Rejected level change mutated hierarchy");
    Reject([&]{parent->SetParent(child);});
    Reject([&]{parent->SetParent(parent);});
    Reject([&]{text->SetFrame(reinterpret_cast<CSimpleFrame*>(uintptr_t(1)),2,1);});
    Reject([&]{child->SetFrameStrata(UINT32_MAX);});
    Reject([&]{child->SetFrameLevel(INT32_MAX,1);});
    Reject([&]{child->SetFrameScale(std::numeric_limits<float>::infinity(),false);});
    Reject([&]{child->SetFrameScale(0,false);});
    Reject([&]{child->EnableEvent(SIMPLE_EVENT_MOUSE,0);});
    Reject([&]{child->LoadXML(nullptr,nullptr);});
    Reject([&]{child->SetName("UnavailableFrame");});
    bool foreign=false;std::thread thread([&]{try{child->Hide();}catch(const std::logic_error&){foreign=true;}});thread.join();Check(foreign,"Foreign Frame mutation accepted");
    EVENT_DATA_SIZE size{};CSimpleTop::OnDisplaySizeChanged(&size,&root);
    Equal(first,Draw(device,root)); // Direct event injection, not SDL acceptance.
    child->SetParent(nullptr);Check(!parent->m_children.Head() && !child->m_parent,"Frame detach left child node");
    child->SetParent(parent);Equal(first,Draw(device,root));
    if(!snapshot.empty()) {
        std::ofstream out(snapshot,std::ios::binary);out<<"P6\n640 480\n255\n";
        for(int y=479;y>=0;--y)for(int x=0;x<640;++x){const auto& p=first[y*640+x];char rgb[]={char(p.r),char(p.g),char(p.b)};out.write(rgb,3);}
        Check(bool(out),"Frame root snapshot failed");
    }
    int deleted=0;new UpdateFrame(parent,deleted);
    root.OnLayerUpdate(0.016f);Check(deleted==0,"Frame destroyed within layer traversal");
    root.OnLayerUpdate(0.016f);Check(deleted==1,"Deferred Frame was not destroyed before the next update");
    Frame::Native::DestroyLater(parent);Frame::Native::DestroyLater(child);
    root.OnLayerUpdate(0.016f);Check(!root.m_frames.Head() && !root.m_destroyed.Head() && CSimpleFrame::s_frameCount==0,"Parent destruction left Frames/attribute cleanup");
    Check(!Ink(Draw(device,root)),"Destroyed Frame retained rendered glyphs");
    // Root destruction also owns any Frames that remain attached to it.
    new CSimpleFrame(nullptr);
    std::cout<<"frame-parent-child-layer-root-visibility-reparent-alpha-scale-deferred=passed ink="<<ink<<'\n';
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
                GlContext context("Original Frame parent/root fixture",640,480,true);
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
                    Hierarchy(device,*selected,cycle?"":snapshot);
                    renderer.CheckCanClose();factories.CheckCanClose();}
                Check(!source.queue.Pending(),"Region left pending file work");
            }
            Check(!s_context && !OsLiveThreadCount(),"Region shutdown left VM/workers");std::cout<<"frame-hierarchy-cycle-closed="<<cycle+1<<" workers=0\n";
        }
        OsTimeShutdown();StormRtlDestroy();return 0;
    }catch(const std::exception& error){std::cerr<<"NativeFrameHierarchyCheck failed: "<<error.what()<<'\n';if(started){OsTimeShutdown();StormRtlDestroy();}return 1;}
}
