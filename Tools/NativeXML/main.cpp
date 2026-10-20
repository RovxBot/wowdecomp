#include "Engine/Source/FrameXML/Portable/Runtime.h"
#include "Engine/Source/FrameXML/XMLTree.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/Services/Portable/AsyncFile.h"
#include "Engine/Source/Os/Portable/Runtime.h"
#include "Engine/Source/Storm/Source/MD5.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <thread>

using Bytes = std::vector<uint8_t>;
using namespace Services::Native;
using FrameScript::Native::SourceKind;
static void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
static Bytes Load(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    Check(bool(file), "Missing authored fixture");
    return Bytes(std::istreambuf_iterator<char>(file), {});
}
static std::string Hex(const uint8_t* data, size_t size) {
    std::string value;
    for (size_t i=0;i<size;++i) { value += "0123456789abcdef"[data[i]>>4]; value += "0123456789abcdef"[data[i]&15]; }
    return value;
}
static XMLTree* Parse(const std::string& source) {
    return XMLTree_Load(source.data(), static_cast<uint32_t>(source.size()));
}
static int released;
struct UserData final : XMLNodeData {
    ~UserData() override { ++released; }
    void Release() override { delete this; }
};
static void Trees() {
    auto* tree=Parse("<Ui name=\"alpha\"><Script>  hello <Child/>world &amp; text</Script><Empty/></Ui>");
    Check(tree,"original XML parsing failed");
    auto* root=XMLTree_GetRoot(tree);
    Check(root && !std::strcmp(root->m_name.m_str,"Ui") && !std::strcmp(root->GetAttributeByName("NAME"),"alpha"),"XML attributes/case lookup");
    auto* script=root->GetChildByName("script");
    Check(script && !std::strcmp(script->m_body,"  hello world & text") && script->m_child->m_offset==8,"mixed XML body/child offset");
    root->SetAttribute("NAME","beta"); root->SetAttribute("other","value");
    Check(root->m_attributes.Count()==2 && !std::strcmp(root->GetAttributeByName("name"),"beta"),"attribute replacement");
    auto* detached=root->GetChildByName("empty"); detached->Detach(); delete detached;
    Check(!script->m_next && !root->GetChildByName("Empty"),"XML node detach");
    released=0; root->m_userData=new UserData;
    bool busy=false;
    try { XMLTree_Cleanup(); } catch (const std::logic_error&) { busy=true; }
    Check(busy,"cleanup accepted a live borrowed tree");
    bool foreign=false;
    std::thread thread([&] { try { root->SetAttribute("name","foreign"); } catch (const std::logic_error&) { foreign=true; } });
    thread.join(); Check(foreign && !std::strcmp(root->GetAttributeByName("name"),"beta"),"foreign XML mutation");
    XMLTree_Free(tree); Check(released==1,"XML user data Release count");
    for (const auto& source : {std::string(""),std::string("<Ui><broken></Ui>"),std::string("<Ui a='1' a='2'/>"),
                              std::string("<!DOCTYPE Ui [<!ENTITY a 'x'>]><Ui>&a;</Ui>"), std::string("<Ui>\0</Ui>",10),
                              std::string(8*1024*1024u+1,'x')})
        Check(!Parse(source),"invalid/unsupported XML accepted");
    Check(!XMLTree_Load(nullptr,1),"null XML bytes accepted");
    std::string deep;
    for (int i=0;i<257;++i) deep += "<N>";
    for (int i=0;i<257;++i) deep += "</N>";
    Check(!Parse(deep),"XML depth limit lost");
    // Wide lists previously recursed once per sibling during Recycle.
    std::string wide="<Ui>";
    for (int i=0;i<20000;++i) wide += "<N a='odd' b='even'/>";
    wide += "</Ui>";
    tree=Parse(wide); Check(tree,"wide XML list"); XMLTree_Free(tree);
    tree=Parse("<Ui><AfterFailure/></Ui>"); Check(tree && XMLTree_GetRoot(tree)->GetChildByName("AfterFailure"),"parser/pool recovery"); XMLTree_Free(tree);
    XMLTree_Cleanup();
}
static void Signatures(const std::filesystem::path& directory) {
    const std::vector<std::string> names={"Fixture.toc","main.lua","Panels/Main.xml","Shared.xml","scripts/nested.lua","Bindings.xml","Missing.toc","Fixture.toc.sig","Missing.toc.sig"};
    std::map<std::string,Bytes> files;
    for (auto name : names) {
        std::string key=name; std::replace(key.begin(),key.end(),'/','\\');
        files[key]=Load(directory/name);
    }
    auto modulus=Load(directory/"modulus.bin"), expected=Load(directory/"expected.bin");
    FileQueue queue;
    size_t completed=0;
    auto reader=[&](SourceKind kind, const std::string& path, Bytes& bytes) {
        Check(kind==SourceKind::Interface,"XML attempted saved-variable fallback");
        if (!files.count(path)) return false;
        auto result=std::make_shared<ReadResult>();
        // Actual retained worker/callback ownership, using authored bytes here.
        Bytes content=files.at(path);
        auto ticket=queue.Submit([content=std::move(content)](const Cancellation&) {
            ReadResult result; result.file.bytes=content; return result;
        },[&completed,result](const ReadResult& value) { ++completed; *result=value; });
        if (!queue.Wait(ticket,std::chrono::seconds(5))) { queue.Cancel(ticket); return false; }
        bytes=std::move(result->file.bytes); return result->status==ReadStatus::Success;
    };
    FrameScript::Native::Runtime scripts(reader);
    FrameXML::Native::Runtime xml;
    bool reset=false, duplicate=false;
    try { FrameScript_Flush(); } catch (const std::logic_error&) { reset=true; }
    try { FrameXML::Native::Runtime other; } catch (const std::logic_error&) { duplicate=true; }
    Check(reset && duplicate,"XML/VM ownership pin");
    Trees();
    uint8_t digest[16];
    auto signature=[&](const char* name="Fixture.toc",const char* extra="Bindings.xml",const uint8_t* key=nullptr) {
        return FrameXML_CheckSignature(name,extra,key ? key : modulus.data(),digest);
    };
    Check(signature()==3 && std::equal(expected.begin(),expected.end(),digest),"independent signed traversal known answer");
    auto stats=FrameXML::Native::LastTraversal();
    Check(stats.files==8 && completed==9,"duplicate/include/extra digest ordering");
    Check(signature("Fixture.toc","Bindings.xml",FrameXML::Native::OriginalInterfaceKey())==1,"synthetic key entered application trust anchor");
    Check(signature("Missing.toc",nullptr)==2,"incomplete traversal authenticated a signed prefix");
    Check(signature("Fixture.toc","absent.xml")==2,"missing extra file accepted");
    Check(signature("absent.toc")==0,"missing sidecar status");
    files["Renamed.toc.sig"]=files["Fixture.toc.sig"];
    Check(signature("Renamed.toc")==1,"RSA filename binding lost");
    for (size_t offset : {size_t(0),size_t(16),size_t(275)}) {
        files["Fixture.toc.sig"][offset]^=1;
        Check(signature()==1,"corrupt signed digest/footer/RSA accepted");
        files["Fixture.toc.sig"][offset]^=1;
    }
    auto original=files["Fixture.toc.sig"]; files["Fixture.toc.sig"].pop_back();
    Check(signature()==1,"truncated sidecar accepted"); files["Fixture.toc.sig"]=original;
    auto lua=files["main.lua"]; files["main.lua"].push_back(' ');
    Check(signature()==2,"modified interface content accepted"); files["main.lua"]=lua;
    auto shared=files["Shared.xml"];
    for (const char* source : {"<Ui><Include file=\"Shared.xml\"/></Ui>","<Ui><Include file=\"../../escape.xml\"/></Ui>","<Ui><Script file=\"absent.lua\"/></Ui>"}) {
        files["Shared.xml"]=Bytes(source,source+std::strlen(source));
        Check(signature()==2,"cycle/path escape/missing child accepted");
    }
    files["Shared.xml"]=shared;
    CStatus status; MD5_CTX md5; MD5Init(&md5);
    auto* tree=FrameXML_LoadXML("Panels\\Main.xml",&md5,&status);
    Check(tree && XMLTree_GetRoot(tree)->GetChildByName("Include"),"completed XML source read"); XMLTree_Free(tree);
    MD5Final(digest,&md5);
    auto expectedXML=Blizzard::Mopaq::Native::Digest(files["Panels\\Main.xml"]);
    Check(std::equal(expectedXML.begin(),expectedXML.end(),digest),"load XML raw digest");
    Check(!FrameXML_LoadXML("absent.xml",nullptr,&status),"missing XML succeeded");
    Check(signature()==3 && !queue.Pending(),"post-failure interface verification recovery");
}
static void Original(const char* data, const char* settings, const char* cache) {
    for (int cycle=0;cycle<3;++cycle) {
        {
            Blizzard::File::Native::Paths paths(data,settings,cache);
            std::shared_ptr<Blizzard::Mopaq::Native::Packages> packages;
            FileQueue queue;
            auto open=queue.Submit([&](const Cancellation&) {
                packages=Blizzard::Mopaq::Native::Packages::Open12340(paths,"enUS"); return ReadResult{};
            },[](const ReadResult& value) { Check(value.status==ReadStatus::Success,"original archive authentication"); });
            if (!queue.Wait(open,std::chrono::seconds(60))) { queue.Shutdown(); throw std::runtime_error("original package timeout"); }
            std::cout << "original-signaturefile=authenticated records=" << packages->SignatureCount() << '\n';
            auto reader=[&](SourceKind kind,const std::string& path,Bytes& bytes) {
                Check(kind==SourceKind::Interface,"original XML requested saved variables");
                auto result=std::make_shared<ReadResult>();
                auto ticket=queue.ReadPackage(packages,path,[result](const ReadResult& value) { *result=value; });
                if (!queue.Wait(ticket,std::chrono::seconds(60))) { queue.Cancel(ticket); return false; }
                if (result->status!=ReadStatus::Success) return false;
                bytes=std::move(result->file.bytes); return true;
            };
            FrameScript::Native::Runtime scripts(reader);
            FrameXML::Native::Runtime xml;
            for (const char* toc : {"Interface\\GlueXML\\GlueXML.toc","Interface\\FrameXML\\FrameXML.toc"}) {
                uint8_t digest[16];
                const char* extra=std::strstr(toc,"FrameXML.toc") ? "Interface\\FrameXML\\Bindings.xml" : nullptr;
                int result=FrameXML_CheckSignature(toc,extra,FrameXML::Native::OriginalInterfaceKey(),digest);
                auto stats=FrameXML::Native::LastTraversal();
                std::cout << "original-interface=" << toc << " result=" << result << " files=" << stats.files << " bytes=" << stats.bytes << " md5=" << Hex(digest,16) << '\n';
                Check(result==3,"original filename-bound RSA/content digest verification");
            }
            CStatus status;
            auto* tree=FrameXML_LoadXML("Interface\\GlueXML\\AccountLogin.xml",nullptr,&status);
            Check(tree && XMLTree_GetRoot(tree) && !std::strcmp(XMLTree_GetRoot(tree)->m_name.m_str,"Ui"),"original AccountLogin XML tree");
            XMLTree_Free(tree);
            Check(!queue.Pending(),"original XML read callback survived");
        }
        Check(!s_context && !OsLiveThreadCount(),"original XML/VM/worker teardown");
        std::cout << "original-xml-cycle=" << cycle << " workers=0\n";
    }
}
int main(int argc,char** argv) {
    try {
        if (argc==5 && !std::strcmp(argv[1],"--original-data")) { Original(argv[2],argv[3],argv[4]); return 0; }
        Check(argc==2,"Usage: NativeXMLCheck FIXTURES | --original-data DATA SETTINGS CACHE");
        for (int cycle=0;cycle<3;++cycle) {
            Signatures(std::filesystem::u8path(argv[1]));
            Check(!s_context && !OsLiveThreadCount(),"XML/VM/worker teardown");
            std::cout << "XML/signature authored cycle=" << cycle << " workers=0\n";
        }
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
