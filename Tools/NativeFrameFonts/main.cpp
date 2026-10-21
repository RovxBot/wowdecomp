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
static void Lua(const char* source) {
    CStatus status;
    int result=FrameScript_ExecuteBuffer(source,std::strlen(source),"FrameFontFixture",&status,nullptr);
    if (!result || !status.IsEmpty()) {char error[2048];throw std::runtime_error(status.GetErrorStr(error,sizeof(error),STATUS_WARNING,"; "));}
}
struct Script final : CScriptObject {
    CScriptObject* parent=nullptr;
    int32_t meta;
    explicit Script(int32_t table) : meta(table) {}
    CScriptObject* GetScriptObjectParent() override { return parent; }
    int32_t GetScriptMetaTable() override { return meta; }
};
struct Consumer final : CSimpleFontable {
    CSimpleFontStringAttributes attributes;
    int updates=0;
    bool mutationProbe=false;
    void FontObjectUpdated(CSimpleFontStringAttributes& source) override {
        ++updates;source.Update(attributes,m_fontableFlags);
        if (mutationProbe) Reject([&]{SetFontObject(nullptr);});
    }
};
static void ScriptObjects(Frame::Native::Fonts& owner) {
    auto* L=FrameScript_GetContext();
    int meta=FrameScript_Object::CreateScriptMetaTable(L,&CScriptObject::RegisterScriptMethods);
    {
        Script parent(meta),child(meta);
        parent.SetName("Parent");child.parent=&parent;child.SetName("$parentLabel");
        Check(CScriptObject::GetScriptObjectByName("ParentLabel",CScriptObject::GetObjectType())==&child,"Named script lookup");
        Check(!CScriptObject::GetScriptObjectByName("BaseFont",CScriptObject::GetObjectType()),"Font dispatched as CScriptObject");
        Lua("assert(ParentLabel:GetName()=='ParentLabel' and ParentLabel:GetParent()==Parent); assert(ParentLabel:GetObjectType()=='Object'); SavedObject=ParentLabel");
        child.SetName("Renamed");
        Lua("assert(ParentLabel==nil and Renamed~=SavedObject and SavedObject[0]==nil)");
        char small[4]; child.parent=nullptr;child.CreateName("$parentSuffix",small,sizeof(small));
        Check(!std::strcmp(small,"Top"),"Bounded default parent name");
        Reject([&]{owner.CheckCanClose();});
    }
    Lua("assert(Renamed==nil and Parent==nil); assert(not pcall(SavedObject.GetName,SavedObject))");
    luaL_unref(L,LUA_REGISTRYINDEX,meta);
}
static void Authored(Frame::Native::Fonts& owner,const char* face) {
    Lua("assert(CreateFont('BaseFont')==BaseFont);assert(CreateFont('BaseFont')==BaseFont);assert(not pcall(CreateFont,''))");
    auto* base=CSimpleFont::GetFont("BaseFont",1);
    auto* child=CSimpleFont::GetFont("ChildFont",1);
    auto* copied=CSimpleFont::GetFont("CopyFont",1);
    Check(base==CSimpleFont::GetFont("basefont",1) && !CSimpleFont::GetFont("absent",0),"Original named font hash");
    base->GetObjectType();
    Check(base->m_attributes.SetFont(face,NDCToDDCHeight(32.0f/480),0),"Actual Services font validation");
    Lua(R"LUA(
assert(BaseFont:GetObjectType()=='Font' and BaseFont:IsObjectType('Font'))
assert(BaseFont:GetName()=='BaseFont')
ChildFont:SetFontObject(BaseFont)
assert(ChildFont:GetFontObject()==BaseFont)
local name,height,flags=BaseFont:GetFont()
assert(name and height>0 and flags=='')
BaseFont:SetTextColor(1,0,0,1)
assert(ChildFont:GetTextColor()>0.99)
ChildFont:SetTextColor(0,1,0,1)
BaseFont:SetTextColor(0,0,1,1)
local r,g,b=ChildFont:GetTextColor();assert(r==0 and g>0.99 and b==0)
BaseFont:SetJustifyH('LEFT');BaseFont:SetJustifyV('TOP')
assert(BaseFont:GetJustifyH()=='LEFT' and BaseFont:GetJustifyV()=='TOP')
BaseFont:SetIndentedWordWrap(true);assert(BaseFont:GetIndentedWordWrap())
BaseFont:SetShadowColor(0,0,0,1);BaseFont:SetShadowOffset(2,-3)
local x,y=BaseFont:GetShadowOffset();assert(math.abs(x-2)<0.001 and math.abs(y+3)<0.001)
BaseFont:SetSpacing(2);assert(math.abs(BaseFont:GetSpacing()-2)<0.001)
BaseFont:SetAlpha(0.5);assert(math.abs(BaseFont:GetAlpha()-127/255)<0.001)
CopyFont:CopyFontObject(BaseFont);assert(CopyFont:GetFont()==BaseFont:GetFont())
CopyFont:CopyFontObject('BaseFont')
assert(not pcall(BaseFont.SetFontObject,BaseFont,ChildFont))
assert(not pcall(BaseFont.SetFontObject,BaseFont,BaseFont))
assert(not pcall(BaseFont.SetFontObject,BaseFont,{}))
assert(not pcall(BaseFont.SetFontObject,BaseFont,{[0]=BaseFont[0]}))
assert(not pcall(BaseFont.GetName,{[0]=BaseFont[0]}))
assert(not pcall(BaseFont.CopyFontObject,BaseFont,{[0]=BaseFont[0]}))
assert(not pcall(BaseFont.SetAlpha,BaseFont,0/0))
assert(not pcall(BaseFont.SetAlpha,BaseFont,2))
assert(not pcall(BaseFont.SetTextColor,BaseFont,0/0,0,0))
assert(not pcall(BaseFont.SetSpacing,BaseFont,1/0))
assert(not pcall(BaseFont.SetShadowOffset,BaseFont,1/0,0))
assert(not pcall(BaseFont.SetIndentedWordWrap,BaseFont,1/0))
assert(not BaseFont:SetFont('bad.ttf',12))
)LUA");
    {
        Consumer consumer;
        consumer.SetFontObject(base);Check(consumer.updates==1,"Fontable initial update");
        consumer.mutationProbe=true;
        base->m_attributes.m_flags|=0x404;base->UpdateObjects();
        Check(consumer.updates==2,"Fontable registered list update");
        Reject([&]{owner.CheckCanClose();});
        consumer.mutationProbe=false;consumer.SetFontObject(nullptr);
    }
    Reject([&]{base->SetFontObject(child);});
    Reject([&]{CSimpleFont::GetFont("",1);});
    // Direct constructors remain owner-bound and must close before the factory.
    {CSimpleFont standalone;Reject([&]{owner.CheckCanClose();});}
    bool foreign=false;
    std::thread thread([&]{try{base->m_attributes.SetFont(face,0.1f,0);}catch(const std::logic_error&){foreign=true;}});
    thread.join();Check(foreign,"Foreign font mutation accepted");
    auto* L=FrameScript_GetContext();
    lua_newtable(L);lua_pushlightuserdata(L,reinterpret_cast<void*>(uintptr_t(1)));lua_rawseti(L,-2,0);lua_setglobal(L,"Forged");
    Lua("assert(not pcall(BaseFont.GetName,Forged))");
    ScriptObjects(owner);
    // New edges and CopyFontObject validate descendants before list mutation.
    CSimpleFont* previous=nullptr;
    for (int i=0;i<64;++i) {
        auto name="Deep"+std::to_string(i);
        auto* node=CSimpleFont::GetFont(name.c_str(),1);node->SetFontObject(previous);previous=node;
    }
    auto* overflow=CSimpleFont::GetFont("DeepOverflow",1);
    Reject([&]{overflow->SetFontObject(previous);});
    Reject([&]{CSimpleFont::GetFont("Deep0",0)->SetFontObject(overflow);});
    Lua("assert(not pcall(BaseFont.CopyFontObject,BaseFont,Deep63))");
    Lua("assert(not pcall(DeepOverflow.SetFontObject,DeepOverflow,Deep63));assert(not pcall(BaseFont.SetFontObject,BaseFont,''))");
    // Destroy registered parents while children and external Lua tables remain.
    Lua("OldFont=BaseFont");
    CSimpleFont::Destroy();
    Lua("assert(BaseFont==nil and ChildFont==nil and CopyFont==nil);assert(not pcall(OldFont.GetName,OldFont))");
    Check(!CSimpleFont::GetFont("BaseFont",0),"Factory cache survived destruction");
    // Rebuild and apply actual XML dimensions/color/inheritance, not a proxy font.
    const std::string source="<Ui><Font name='XMLBase' font='"+std::string(face)+"' justifyH='LEFT'><FontHeight><AbsValue val='32'/></FontHeight><Color r='1' g='0' b='0'/></Font><Font name='XMLChild' inherits='XMLBase' spacing='1'><Shadow><Color r='0' g='0' b='0'/><Offset x='1' y='-1'/></Shadow></Font></Ui>";
    std::unique_ptr<XMLTree,decltype(&XMLTree_Free)> tree(XMLTree_Load(source.data(),uint32_t(source.size())),&XMLTree_Free);
    Check(bool(tree),"Authored font XML parse");
    CStatus status;
    for (auto* node=XMLTree_GetRoot(tree.get())->m_child;node;node=node->m_next)
        CSimpleFont::GetFont(node->GetAttributeByName("name"),1)->LoadXML(node,&status);
    tree.reset();Check(status.IsEmpty(),"Authored font XML warnings");
    Lua("assert(XMLChild:GetFontObject()==XMLBase and XMLChild:GetFont()==XMLBase:GetFont());assert(math.abs(XMLChild:GetSpacing()-1)<0.001)");
    Check(CSimpleFont::GetFont("XMLChild",0)->m_attributes.m_fontHeight>0,"XML inherited font height");
}
struct Handle {
    HOBJECT value;
    ~Handle(){if(value)HandleClose(value);}
};
struct Batch {
    CGxStringBatch* value=GxuFontCreateBatch(false,false);
    ~Batch(){GxuFontDestroyBatch(value);}
};
static size_t Render(LegacyDevice& device,CSimpleFont& font,const std::string& snapshot) {
    auto& a=font.m_attributes;
    HTEXTFONT hfont=TextBlockGenerateFont(a.m_font.m_str,a.m_fontFlags,a.m_fontHeight);
    Check(hfont,"Frame attributes did not generate Services font");
    Handle ownedFont{reinterpret_cast<HOBJECT>(hfont)};
    // Text blocks borrow the font: close blocks/batch before the font handle.
    HTEXTBLOCK block=TextBlockCreate(hfont,"AV Frame",CImVector(0,0,255,255),C3Vector(0.1f,0.1f,0),
        a.m_fontHeight,0.6f,0.4f,0x21,0,0,1);
    Check(block,"Frame Services block failed");
    Handle ownedBlock{reinterpret_cast<HOBJECT>(block)};
    Batch batch;
    Check(GxuFontAddToBatch(batch.value,TextBlockGetStringPtr(block)),"Frame text batch failed");
    GxSceneClear(GxBufferBit_Color,CImVector(0,0,0,255));GxuFontRenderBatch(batch.value);
    CiRect rect{};rect.maxX=640;rect.maxY=480;TSGrowableArray<CImVector> pixels;
    device.DeviceReadPixels(rect,pixels);
    size_t red=0,ink=0;uint8_t peak=0;
    for(uint32_t i=0;i<pixels.Count();++i) {
        const auto& p=pixels[i];red+=p.r>240 && p.g<4 && p.b<4;
        ink+=p.r>16 && p.g<4 && p.b<4;peak=std::max(peak,p.r);
    }
    std::cout<<"frame-font-pixels font="<<font.GetName()<<" height-ddc="<<a.m_fontHeight
             <<" flags="<<a.m_fontFlags<<" fully-red="<<red<<" red-ink="<<ink<<" peak-red="<<int(peak)<<'\n';
    if (!snapshot.empty()) {
        std::ofstream out(snapshot,std::ios::binary);out<<"P6\n640 480\n255\n";
        for (int y=479;y>=0;--y)for(int x=0;x<640;++x){const auto& p=pixels[y*640+x];char rgb[]={char(p.r),char(p.g),char(p.b)};out.write(rgb,3);}
        Check(bool(out),"Frame font snapshot write failed");
    }
    Check(red>4,"Frame font attributes did not render original Gxu glyphs");
    return red;
}
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
int main(int argc,char** argv) {
    bool started=false;
    try {
        std::string data,settings,cache,font,snapshot;
        for(int i=1;i<argc;++i){Check(i+1<argc,"Missing option value");std::string key=argv[i],value=argv[++i];
            if(key=="--data")data=value;else if(key=="--settings")settings=value;else if(key=="--cache")cache=value;
            else if(key=="--font")font=value;else if(key=="--snapshot")snapshot=value;else throw std::invalid_argument("Unknown Frame font option");}
        Check(data.empty()==settings.empty() && data.empty()==cache.empty(),"Separate original data/user roots required");
        Check(!data.empty() || !font.empty(),"Supply authored font or original data");
        Check(!std::fesetenv(FE_DFL_ENV) && !std::fesetround(FE_TONEAREST),"FP initialization failed");
        StormRtlInitialize();OsTimeStartup(OsTimeManager::BestAvailable);started=true;
        for(int cycle=0;cycle<3;++cycle) {
            {
                GlContext context("Original Frame font factory fixture",640,480,true);
                std::cout<<"gl-renderer="<<context.Info().renderer<<"\ngl-version="<<context.Info().version<<'\n';
                Source source(data,settings,cache,font);
                LegacyDevice device(context,[&](const std::string& path){return source.shaders.at(path);});
                GxRsSet(GxRs_Lighting,0);GxRsSet(GxRs_Fog,0);
                FrameScript::Native::Runtime scripts([&](FrameScript::Native::SourceKind kind,const std::string& path,Bytes& bytes){return kind==FrameScript::Native::SourceKind::Interface && source.Read(path,bytes);});
                FrameXML::Native::Runtime xml;
                Frame::Native::Runtime layout;
                Gxu::Native::LegacyFonts gxu([&](const std::string& path){return source.Font(path);});
                {
                    Frame::Native::Fonts factories;
                    Reject([]{Frame::Native::Fonts duplicate;});
                    Reject([]{Frame::Native::RequireNoObjects();});
                    Reject([]{Gxu::Native::RequireNoFontConsumers();});
                    if(data.empty())Authored(factories,"Fonts\\fixture.ttf");
                    CSimpleFont* selected=data.empty()?CSimpleFont::GetFont("XMLBase",0):Original(source);
                    size_t red=Render(device,*selected,cycle?"":snapshot);
                    factories.CheckCanClose();
                    std::cout<<"frame-font-factory-inheritance-xml-lua-render=passed red="<<red<<" original-data="<<!data.empty()<<'\n';
                }
                Check(CSimpleFont::s_metatable==LUA_NOREF && !source.queue.Pending(),"Font factory/queue teardown");
            }
            Check(!s_context && !OsLiveThreadCount(),"Frame font shutdown left VM/workers");
            std::cout<<"frame-font-cycle-closed="<<cycle+1<<" workers=0\n";
        }
        OsTimeShutdown();StormRtlDestroy();return 0;
    } catch(const std::exception& error) {
        std::cerr<<"NativeFrameFontsCheck failed: "<<error.what()<<'\n';
        if(started){OsTimeShutdown();StormRtlDestroy();}return 1;
    }
}
