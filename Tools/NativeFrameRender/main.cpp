#include "Engine/Source/Frame/Portable/Render.h"
#include "Engine/Source/Frame/CRenderBatch.h"
#include "Engine/Source/Frame/CSimpleRender.h"
#include "Engine/Source/Services/Portable/Texture.h"
#include "Engine/Source/Services/Texture.h"
#include "Engine/Source/Gx/CGxDevice/CGxShader.h"
#include "Engine/Source/Gx/Portable/LegacyDevice.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Gxu/GxuFont.h"
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
#include <limits>
#include <memory>
#include <stdexcept>
#include <thread>
using Bytes = std::vector<uint8_t>;
using namespace Gx::Native;
using namespace Services::Native;
static void Check(bool value, const char* why) { if (!value) throw std::runtime_error(why); }
template<class F> static void Reject(F f) {
    bool rejected=false; try { f(); } catch (const std::logic_error&) { rejected=true; }
    Check(rejected,"Invalid Frame render operation accepted");
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
struct Handle {
    HOBJECT value=nullptr;
    explicit Handle(HOBJECT h) : value(h) {}
    ~Handle(){Close();}
    void Close(){if(value){HandleClose(value);value=nullptr;}}
    Handle(const Handle&)=delete;
    Handle& operator=(const Handle&)=delete;
};
struct PixelShader {
    LegacyDevice& device;
    CGxShader* value=nullptr;
    explicit PixelShader(LegacyDevice& d) : device(d) { device.ShaderCreate(&value,GxSh_Pixel,"Shaders\\Pixel","UI",1); }
    ~PixelShader(){device.ShaderDestroy(value);}
};
static std::vector<CImVector> Pixels(LegacyDevice& device) {
    CiRect rect{};rect.maxX=640;rect.maxY=480;TSGrowableArray<CImVector> pixels;
    device.DeviceReadPixels(rect,pixels);return {pixels.Ptr(),pixels.Ptr()+pixels.Count()};
}
static void Snapshot(const std::vector<CImVector>& pixels,const std::string& path) {
    if(path.empty())return;
    std::ofstream file(path,std::ios::binary);file<<"P6\n640 480\n255\n";
    for(int y=479;y>=0;--y)for(int x=0;x<640;++x){auto& p=pixels[y*640+x];char rgb[]={char(p.r),char(p.g),char(p.b)};file.write(rgb,3);}
    Check(bool(file),"Frame batch snapshot write failed");
}
static void Quad(CRenderBatch& batch,CTexture* texture,CGxShader* shader,float left=-1,float right=1) {
    C3Vector positions[]={{left,1,0},{left,-1,0},{right,1,0},{right,-1,0}};
    C2Vector coords[]={{0,0},{0,1},{1,0},{1,1}};
    uint16_t indices[]={0,1,2,2,1,3};
    batch.Queue(texture,GxBlend_Opaque,4,positions,coords,0,nullptr,6,indices,shader);
    // These stack arrays go away before render; batch submission must own copies.
    for(auto& p:positions)p=C3Vector(0,0,0);
    for(auto& t:coords)t=C2Vector(0,0);
    std::fill(std::begin(indices),std::end(indices),uint16_t(0));
}
static void ClearScreen() {GxSceneClear(GxBufferBit_Color,CImVector(0,0,0,255));}
static void Solid(const std::vector<CImVector>& pixels,size_t index,uint8_t r,uint8_t g,uint8_t b) {
    const auto& p=pixels.at(index);
    Check(p.r==r && p.g==g && p.b==b,"Frame batch pixel mismatch");
}
static Bytes AtlasBlp() {
    Bytes bytes(148+8*8*4);
    auto word=[&](size_t at,uint32_t n){for(int b=0;b<4;++b)bytes[at+b]=uint8_t(n>>(b*8));};
    word(0,0x32504c42);word(4,0x11);bytes[8]=3;bytes[9]=8;
    word(12,8);word(16,8);word(20,148);word(84,8*8*4);
    for(size_t i=148;i<bytes.size();i+=4){bytes[i]=255;bytes[i+3]=255;}
    return bytes;
}
struct CallbackProbe {
    CRenderBatch& batch;
    int calls=0;
    static void Run(void* value) {
        auto& self=*static_cast<CallbackProbe*>(value);++self.calls;
        Reject([&]{self.batch.Clear();});
        Reject([&]{self.batch.Finish();});
        Reject([&]{CSimpleRender::DrawBatch(&self.batch);});
        Reject([&]{self.batch.QueueCallback(&Run,&self);});
    }
    static void Fail(void*) {throw std::logic_error("Authored render callback failure");}
};
static void Sorting() {
    CRenderBatch batch;
    // Synthetic metadata exercises full-width keys without dereferencing them.
    const uintptr_t values[]={0x200000010ULL,0x100000010ULL,0x300000010ULL,0x100000001ULL};
    for(auto value:values){auto* mesh=batch.m_texturelist.New();*mesh={};mesh->m_textureID=reinterpret_cast<CGxTex*>(value);}
    batch.Finish();
    for(uint32_t i=1;i<batch.m_texturelist.Count();++i)
        Check(reinterpret_cast<uintptr_t>(batch.m_texturelist[i-1].m_textureID)<reinterpret_cast<uintptr_t>(batch.m_texturelist[i].m_textureID),"Frame texture sort truncated pointer keys");
    batch.Clear();
    for(auto value:values){auto* mesh=batch.m_texturelist.New();*mesh={};mesh->m_shader=reinterpret_cast<CGxShader*>(value);}
    batch.Finish();
    for(uint32_t i=1;i<batch.m_texturelist.Count();++i)
        Check(reinterpret_cast<uintptr_t>(batch.m_texturelist[i-1].m_shader)<reinterpret_cast<uintptr_t>(batch.m_texturelist[i].m_shader),"Frame shader sort truncated pointer keys");
}
static void Meshes(LegacyDevice& device,FileQueue& queue,Frame::Native::Renderer& owner) {
    TextureService service(queue,[](const std::string& name,const Cancellation&){
        ReadResult result;result.file.bytes=AtlasBlp();
        if(name=="INTERFACE\\RED.BLP" || name=="INTERFACE\\WHITE.BLP") {
            auto& bytes=result.file.bytes;bytes[4]=1;
            for(size_t i=148;i<bytes.size();i+=4) {
                bytes[i]=bytes[i+1]=name=="INTERFACE\\WHITE.BLP"?255:0;
                bytes[i+2]=255;
            }
        }
        return result;
    });
    PixelShader shader(device);
    CRenderBatch batch;
    Reject([&]{owner.CheckCanClose();});
    Handle red(reinterpret_cast<HOBJECT>(TextureCreate("Interface\\red.blp",CGxTexFlags(GxTex_Nearest),nullptr,1)));
    auto* texture=reinterpret_cast<CTexture*>(red.value);Check(texture,"Solid texture creation failed");
    const auto sharedShaderRefs=shader.value->m_refCount;
    Quad(batch,texture,shader.value);red.Close();
    Check(shader.value->m_refCount==sharedShaderRefs+1,"Frame mesh failed to add shader reference");
    Check(batch.m_texturelist[0].m_texture->m_refcount==1,"Frame mesh failed to retain texture");
    auto* retainedShader=shader.value;device.ShaderDestroy(shader.value);
    Check(retainedShader->m_refCount==sharedShaderRefs,"Frame mesh failed to retain pixel shader");
    bool foreign=false;std::thread thread([&]{try{batch.Clear();}catch(const std::logic_error&){foreign=true;}});thread.join();Check(foreign,"Foreign batch mutation accepted");
    CallbackProbe probe{batch};batch.QueueCallback(&CallbackProbe::Run,&probe);batch.Finish();
    auto depth=device.m_stackOffsets.Count();
    ClearScreen();CSimpleRender::DrawBatch(&batch);auto pixels=Pixels(device);
    Solid(pixels,240*640+320,255,0,0);Check(probe.calls==1 && device.m_stackOffsets.Count()==depth,"Frame callbacks/state stack");
    ClearScreen();CSimpleRender::DrawBatch(&batch);auto again=Pixels(device);
    Check(!std::memcmp(pixels.data(),again.data(),pixels.size()*sizeof(CImVector)),"Retained Frame mesh changed repeat pixels");
    batch.QueueCallback(&CallbackProbe::Fail,nullptr);Reject([&]{CSimpleRender::DrawBatch(&batch);});
    Check(device.m_stackOffsets.Count()==depth,"Callback failure leaked render state");
    batch.Clear();Check(!batch.m_count && !batch.m_stringbatch && !batch.m_texturelist.Count(),"Frame batch clear");
    PixelShader nextShader(device);
    Handle white(reinterpret_cast<HOBJECT>(TextureCreate("Interface\\white.blp",CGxTexFlags(GxTex_Nearest),nullptr,1)));texture=reinterpret_cast<CTexture*>(white.value);
    C3Vector positions[]={{-1,1,0},{-1,-1,0},{1,1,0},{1,-1,0}};C2Vector coords[]={{0,0},{0,1},{1,0},{1,1}};uint16_t indices[]={0,1,2,2,1,3};
    Reject([&]{batch.Queue(nullptr,GxBlend_Opaque,4,positions,coords,0,nullptr,6,indices,nextShader.value);});
    positions[0].x=std::numeric_limits<float>::infinity();Reject([&]{batch.Queue(texture,GxBlend_Opaque,4,positions,coords,0,nullptr,6,indices,nextShader.value);});positions[0].x=-1;
    Reject([&]{batch.Queue(texture,GxBlend_Opaque,4,positions,coords,0,nullptr,6,indices,CSimpleRender::s_vertexShader[0]);});
    indices[5]=4;Reject([&]{batch.Queue(texture,GxBlend_Opaque,4,positions,coords,0,nullptr,6,indices,nextShader.value);});indices[5]=3;
    Reject([&]{batch.Queue(texture,GxBlend_Opaque,4,positions,coords,3,nullptr,6,indices,nextShader.value);});
    Reject([&]{batch.Queue(texture,GxBlend_ConstantAlpha,4,positions,coords,0,nullptr,6,indices,nextShader.value);});
    Reject([&]{batch.QueueCallback(nullptr,nullptr);});Reject([&]{batch.QueueTexture(nullptr);});
    CImVector green(0,255,0,255);
    batch.Queue(texture,GxBlend_Opaque,4,positions,coords,0,&green,6,indices,nextShader.value);
    green=CImVector(255,0,0,255);
    // Failure after the vertex lock must abort only our write. Preserve the
    // caller's already-locked index stream and allow the same batch to retry.
    auto* callerIndex=GxBufStream(GxPoolTarget_Index,sizeof(uint16_t),6);
    std::memcpy(GxBufLock(callerIndex),indices,sizeof(indices));
    Reject([&]{CSimpleRender::DrawBatch(&batch);});
    Check(!device.m_bufLocked[GxPoolTarget_Vertex] && device.m_bufLocked[GxPoolTarget_Index]==callerIndex && device.m_stackOffsets.Count()==depth,"Frame partial-lock unwind changed caller ownership");
    GxBufUnlock(callerIndex,0);
    ClearScreen();CSimpleRender::DrawBatch(&batch);Solid(Pixels(device),240*640+320,0,255,0);batch.Clear();
    // Merging reaches the original uint16 vertex boundary with actual owned quads.
    CSimpleRender::s_mergeEnable=1;
    // Clip the first group: the boundary needs real geometry submission, while
    // billions of identical fullscreen fragments add no coverage on hosted Mesa.
    for(int i=0;i<16383;++i)Quad(batch,texture,nextShader.value,2,3);
    Quad(batch,texture,nextShader.value);
    std::vector<uint32_t> verts(16384),idx(16384);
    Check(CSimpleRender::CalcBatchCounts(&batch,verts.data(),idx.data())==2 && verts[0]==65532 && verts[1]==4 && idx[0]==98298 && idx[1]==6,"Frame merge boundary");
    ClearScreen();CSimpleRender::DrawBatch(&batch);Solid(Pixels(device),240*640+320,255,255,255);batch.Clear();
    Check(!device.m_bufLocked[GxPoolTarget_Vertex] && !device.m_bufLocked[GxPoolTarget_Index],"Frame merge left buffers locked");
    white.Close();
    // A nonzero atlas cell must be transformed by the original draw path.
    TextureEnableAtlas(1);std::vector<std::unique_ptr<Handle>> members;
    for(int i=0;i<10;++i){auto name="Interface\\batch"+std::to_string(i)+".blp";auto h=std::make_unique<Handle>(reinterpret_cast<HOBJECT>(TextureCreate(name.c_str(),CGxTexFlags(GxTex_Nearest),nullptr,0x17)));Check(h->value,"Atlas fixture handle");Check(TextureGetGxTex(reinterpret_cast<CTexture*>(h->value),1,nullptr),"Atlas fixture upload");members.push_back(std::move(h));}
    auto* atlas=reinterpret_cast<CTexture*>(members.back()->value);Check(TextureOnAtlas(atlas) && atlas->m_atlasBlockIndex==9,"Nonzero atlas cell fixture");
    Quad(batch,atlas,nextShader.value);members.clear();Reject([&]{service.RequireIdle();});
    ClearScreen();CSimpleRender::DrawBatch(&batch);Solid(Pixels(device),240*640+320,0,0,255);batch.Clear();service.RequireIdle();
    std::cout<<"frame-render-mesh-snapshot-retention-sort-merge-atlas=passed\n";
}
static void TextAndOriginal(LegacyDevice& device,Source& source,const std::string& snapshot,bool original) {
    std::unique_ptr<TextureService> service;
    if(original)service=std::make_unique<TextureService>(source.queue,source.packages);
    PixelShader shader(device);
    Handle font(reinterpret_cast<HOBJECT>(TextBlockGenerateFont(original?"Fonts\\FRIZQT__.TTF":"Fonts\\fixture.ttf",0,NDCToDDCHeight(32.0f/480))));
    Check(font.value,"Frame batch font failed");
    auto hfont=reinterpret_cast<HTEXTFONT>(font.value);
    Handle block(reinterpret_cast<HOBJECT>(TextBlockCreate(hfont,"AV Frame batch",CImVector(0,0,255,255),C3Vector(0.1f,0.1f,0),NDCToDDCHeight(32.0f/480),0.6f,0.4f,0x21,0,0,1)));
    Check(block.value,"Frame batch text failed");
    CRenderBatch batch,other;
    batch.NativeQueueText(hfont,reinterpret_cast<HTEXTBLOCK>(block.value));
    Reject([&]{other.NativeQueueText(hfont,reinterpret_cast<HTEXTBLOCK>(block.value));});
    Reject([&]{batch.NativeQueueText(hfont,reinterpret_cast<HTEXTBLOCK>(block.value));});
    batch.Clear();other.NativeQueueText(hfont,reinterpret_cast<HTEXTBLOCK>(block.value));other.Clear();
    batch.NativeQueueText(hfont,reinterpret_cast<HTEXTBLOCK>(block.value));
    block.Close();font.Close(); // Batch alone now retains both original handles.
    if(original){Handle image(reinterpret_cast<HOBJECT>(TextureCreate("Interface\\GLUES\\LOADINGBAR\\Loading-BarFill.blp",CGxTexFlags(GxTex_Linear),nullptr,0x17)));Check(image.value,"Original Frame batch image failed");Quad(batch,reinterpret_cast<CTexture*>(image.value),shader.value,-0.9f,-0.4f);}
    batch.Finish();ClearScreen();CSimpleRender::DrawBatch(&batch);auto first=Pixels(device);
    size_t ink=0;for(auto& p:first)ink+=p.r>16 && p.g<4 && p.b<4;
    Check(ink>4,"Frame batch rendered no text");
    ClearScreen();CSimpleRender::DrawBatch(&batch);auto second=Pixels(device);
    Check(!std::memcmp(first.data(),second.data(),first.size()*sizeof(CImVector)),"Frame text changed repeat pixels");Snapshot(first,snapshot);
    batch.Clear();
    std::cout<<"frame-render-retained-text-original-composition=passed colored-pixels="<<ink<<" original-data="<<original<<'\n';
}
int main(int argc,char** argv) {
    bool started=false;
    try {
        std::string data,settings,cache,font,snapshot;
        for(int i=1;i<argc;++i){Check(i+1<argc,"Missing option value");std::string key=argv[i],value=argv[++i];
            if(key=="--data")data=value;else if(key=="--settings")settings=value;else if(key=="--cache")cache=value;else if(key=="--font")font=value;else if(key=="--snapshot")snapshot=value;else throw std::invalid_argument("Unknown Frame batch option");}
        Check(data.empty()==settings.empty() && data.empty()==cache.empty(),"Separate data/user roots required");Check(!data.empty() || !font.empty(),"Supply authored font or original data");
        Check(!std::fesetenv(FE_DFL_ENV) && !std::fesetround(FE_TONEAREST),"FP initialization failed");
        StormRtlInitialize();OsTimeStartup(OsTimeManager::BestAvailable);started=true;
        for(int cycle=0;cycle<3;++cycle){
            {
                GlContext context("Original Frame render batch fixture",640,480,true);
                std::cout<<"gl-renderer="<<context.Info().renderer<<"\ngl-version="<<context.Info().version<<'\n';
                Source source(data,settings,cache,font);
                LegacyDevice device(context,[&](const std::string& name){return source.shaders.at(name);});
                GxRsSet(GxRs_Lighting,0);GxRsSet(GxRs_Fog,0);GxRsSet(GxRs_Culling,0);
                Gxu::Native::LegacyFonts fonts([&](const std::string& name){return source.Font(name);});
                Reject([]{CRenderBatch missingOwner;});
                {Frame::Native::Renderer render;Reject([]{Frame::Native::Renderer duplicate;});Sorting();Meshes(device,source.queue,render);TextAndOriginal(device,source,cycle?"":snapshot,!data.empty());render.CheckCanClose();}
                Check(!source.queue.Pending(),"Frame batch left reads pending");
            }
            Check(!OsLiveThreadCount(),"Frame batch shutdown left workers");std::cout<<"frame-render-cycle-closed="<<cycle+1<<" workers=0\n";
        }
        OsTimeShutdown();StormRtlDestroy();return 0;
    }catch(const std::exception& error){std::cerr<<"NativeFrameRenderCheck failed: "<<error.what()<<'\n';if(started){OsTimeShutdown();StormRtlDestroy();}return 1;}
}
