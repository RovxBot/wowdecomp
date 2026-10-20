#include "Engine/Source/FrameScript/Portable/Runtime.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/Services/Portable/AsyncFile.h"
#include "Engine/Source/Storm/Source/MD5.h"
#include "Engine/Source/Os/Portable/Runtime.h"
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

using FrameScript::Native::Runtime;
using FrameScript::Native::SourceKind;
using namespace Services::Native;
static void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
static void Run(const char* source) {
    CStatus status;
    if (!FrameScript_ExecuteBuffer(source, std::strlen(source), "@authored-framescript", &status, nullptr)) {
        char error[2048]; status.GetErrorStr(error, sizeof(error), STATUS_ERROR, "\n");
        throw std::runtime_error(error);
    }
    Check(lua_gettop(FrameScript_GetContext()) == 0, "unbalanced FrameScript execution stack");
}
static int meta;
static void Methods(lua_State*) {}
struct Listener final : FrameScript_Object {
    const char* name;
    explicit Listener(const char* value) : name(value) { RegisterScriptObject(name); }
    const char* GetName() override { return name; }
    int32_t GetScriptMetaTable() override { return meta; }
    bool IsA(int32_t type) override { return type == 1; }
    void Handler(const char* source) {
        m_onEvent.m_luaRef = FrameScript_CompileFunction("*OnEvent", "return function(self,event,...) %s end", source, nullptr);
        Check(m_onEvent.m_luaRef > 0, "handler compilation failed");
    }
};
static Listener* second;
static Listener* added;
static bool nested;
static int Mutate(lua_State*) {
    if (!nested) {
        nested = true;
        second->UnregisterScriptEvent("TEST_EVENT");
        added->RegisterScriptEvent("TEST_EVENT");
        FrameScript_SignalEvent(0, "%s%d", "nested", 8);
    }
    return 0;
}
static int ResetFromCallback(lua_State* L) {
    bool rejected = false;
    try { FrameScript_Flush(); } catch (const std::logic_error&) { rejected = true; }
    lua_pushboolean(L, rejected); return 1;
}
static int Formatted(lua_State* L) {
    char bytes[256];
    FrameScript_Sprintf(L, 1, bytes, sizeof(bytes));
    lua_pushstring(L, bytes); return 1;
}
static void LibraryCheck() {
    FrameScript_RegisterFunction("resetFromCallback", ResetFromCallback);
    FrameScript_RegisterFunction("frameFormat", Formatted);
    Run(R"lua(
        assert(abs(-3)==3 and floor(4.9)==4 and abs(sin(30)-0.5)<0.00001)
        assert(strtrim('  hello\r\n')=='hello' and strtrim('   ')=='' and strtrim('')=='')
        local a,b,c=strsplit(':','a:b:c'); assert(a=='a' and b=='b' and c=='c')
        assert(strjoin('-',a,b,c)=='a-b-c' and strconcat(a,b,c)=='abc')
        local s,n=strreplace('a-b-a','a','xy'); assert(s=='xy-b-xy' and n==2)
        assert(not pcall(strreplace,'a','','x'))
        local big=string.rep('x',8192); s,n=strreplace('a','a',big); assert(#s==4096 and n==1)
        assert(strlenutf8('h\195\169')==2)
        setglobal('registeredValue','hello'); assert(getglobal('registeredValue')=='hello')
        assert(frameFormat('%2$s:%1$d:%3$x',7,'test',4294967295)=='test:7:ffffffff')
        assert(frameFormat('%d',0/0)=='-2147483648')
        assert(resetFromCallback())
        assert(date('!*t',0).year==1970 and difftime(100,40)==60)
        assert(type(time{year=2020,month=2,day=29,hour=12})=='number')
        debugprofilestart(); assert(debugprofilestop()>=0)
        local x,y,z=scrub(1,{},'x'); assert(x==1 and y==nil and z=='x')
        assert(type(debugstack())=='string')
    )lua");
}
static void SecurityCheck() {
    Run(R"lua(
        errors={}
        seterrorhandler(function(err) errors[#errors+1]=err; return err end)
        function secureTarget(a,b) return a+b,a*b end
        hookCount=0
        hooksecurefunc('secureTarget',function(a,b) hookCount=hookCount+1; assert(a==2 and b==3) end)
        local sum,product=secureTarget(2,3); assert(sum==5 and product==6 and hookCount==1)
        local x,y=securecall(function() return 3,4 end); assert(x==3 and y==4)
        securecall(function() error('expected securecall failure') end)
        assert(#errors==1 and string.find(errors[1],'expected securecall failure'))
        function cleanFunction() return 42 end
        local good,who=issecurevariable('cleanFunction'); assert(good and who==nil)
        assert(issecure())
    )lua");
    FrameScript_Execute("addonValue=19; addonSecure=issecure()", "@authored-addon", "FixtureAddon");
    Run(R"lua(
        local good,who=issecurevariable('addonValue'); assert(not good and who=='FixtureAddon')
        assert(addonValue==19 and not addonSecure)
    )lua");
    Check(!lua_tainted && !lua_taintexpected && !lua_taintedclosure, "taint leaked out of protected execution");
    CStatus status;
    Check(!FrameScript_ExecuteBuffer("error('expected execution failure')", std::strlen("error('expected execution failure')"), "@failed", &status, nullptr), "execution failure hidden");
    Check(!status.IsEmpty() && lua_gettop(s_context) == 0, "execution failure lost status/stack");
    status.Clear();
    const char* syntax = "local = broken";
    Check(!FrameScript_ExecuteBuffer(syntax, std::strlen(syntax), "@bad-syntax", &status, nullptr), "syntax failure hidden");
    Check(!status.IsEmpty() && lua_gettop(s_context) == 0, "syntax error stack");
    Run("seterrorhandler(function() error('expected handler failure') end)");
    FrameScript_Execute("error('expected handler failure recovery')", "@failed-handler", nullptr);
    Check(!s_inErrorHandler && lua_gettop(s_context) == 0, "error handler did not recover");
    Run("seterrorhandler(function(err) return err end); recovered=true");
}
static void EventCheck() {
    const char* names[] = {"TEST_EVENT", "OTHER_EVENT", nullptr};
    FrameScript_CreateEvents(names, 3);
    meta = FrameScript_Object::CreateScriptMetaTable(s_context, Methods);
    {
        Listener first("First"), other("Second"), late("Late");
        second = &other; added = &late; nested = false;
        first.Handler("assert(self==this and event=='TEST_EVENT'); sequence=sequence..'A'; mutate() ");
        other.Handler("sequence=sequence..'B'");
        late.Handler("assert(self==this and event=='TEST_EVENT'); sequence=sequence..'C'");
        Check(first.RegisterScriptEvent("TEST_EVENT") && other.RegisterScriptEvent("TEST_EVENT"), "event registration");
        Check(first.RegisterScriptEvent("TEST_EVENT") && !first.RegisterScriptEvent("ABSENT"), "duplicate/unknown event");
        FrameScript_RegisterFunction("mutate", Mutate);
        Run("sequence=''; this='old-this'; event='old-event'; arg1='old-arg1'; arg2='old-arg2'");
        FrameScript_SignalEvent(0, "%s%d", "outer", 7);
        Run("assert(sequence=='AA' and this=='old-this' and event=='old-event' and arg1=='old-arg1' and arg2=='old-arg2')");
        FrameScript_SignalEvent(0, "%s%d", "second", 9);
        Run("assert(sequence=='AAAC')");
        Check(!other.IsScriptEventRegistered("TEST_EVENT") && late.IsScriptEventRegistered("TEST_EVENT"), "pending listener changes");
        FrameScript_SignalEvent(UINT32_MAX, nullptr);
        double time = 0; uint32_t count = 0;
        FrameScript_GetEventCPUUsage("TEST_EVENT", &time, &count);
        Check(count == 3 && time >= 0, "nested signal profile counters");
        FrameScript_GetEventCPUUsage("OTHER_EVENT", &time, &count);
        Check(count == 0 && time == 0, "per-event profile lookup");
        bool blocked = false;
        try { FrameScript_Flush(); } catch (const std::logic_error&) { blocked = true; }
        Check(blocked, "VM reset accepted live Frame objects");
        bool affinity = false;
        std::thread foreign([&] { try { first.RegisterScriptEvent("OTHER_EVENT"); } catch (const std::logic_error&) { affinity = true; } });
        foreign.join(); Check(affinity, "foreign listener mutation accepted");
        FrameScript_ResetCPUUsage();
        FrameScript_GetEventCPUUsage("TEST_EVENT", &time, &count);
        Check(count == 0 && time == 0, "profile reset");
    }
    Check(!FrameScript_GetEventListeners("TEST_EVENT")->Head(), "destroyed listeners remain registered");
    Run("assert(First[0]==nil and Second[0]==nil and Late[0]==nil)");
    luaL_unref(s_context, LUA_REGISTRYINDEX, meta);
    meta = 0;
}
static void FileCheck(const std::filesystem::path& directory) {
    CStatus status;
    MD5_CTX expected, actual; MD5Init(&expected); MD5Init(&actual);
    const char source[] = "fromFile=37";
    MD5Update(&expected, reinterpret_cast<const uint8_t*>(source), sizeof(source)-1);
    Check(FrameScript_ExecuteFile("Interface\\Folder\\..\\fixture.lua", nullptr, &actual, &status), "retained source read");
    uint8_t a[16], b[16]; MD5Final(a,&actual); MD5Final(b,&expected);
    Check(!std::memcmp(a,b,16), "source traversal digest changed");
    Run("assert(fromFile==37)");
    Check(FrameScript_ExecuteFile("empty.lua", nullptr, nullptr, &status), "empty source rejected");
    for (auto name : {"..\\fixture.lua", "a\\..\\..\\fixture.lua", "/absolute.lua", "C:\\fixture.lua", "missing.lua", "throw.lua", "oversize.lua"})
        Check(!FrameScript_ExecuteFile(name, nullptr, nullptr, &status), "invalid/failed read accepted");
    Check(lua_gettop(s_context)==0, "file failure changed Lua stack");
    Check(FrameScript_LoadVariables("saved.lua", &status), "saved variable environment");
    Run("assert(Saved.value==73 and Saved.text=='hello')");
    Run("local ok,who=issecurevariable('Saved'); assert(not ok and who=='SavedVariables')");
    Run("Saved={value=73,text='a\\000b\\n\\\"',array={1,2},bad=0/0}; Saved.self=Saved");
    auto output = (directory/"serialized.lua").u8string();
    auto file = OsCreateFile(output.c_str(), OS_GENERIC_WRITE, 0, OS_CREATE_ALWAYS, OS_FILE_ATTRIBUTE_NORMAL, 0);
    Check(file != OS_INVALID_FILE, "saved variable output open");
    Check(FrameScript_PrintVariable(file,"Saved") && OsCloseFile(file), "saved variable serialization");
    Check(!FrameScript_PrintVariable(OS_INVALID_FILE,"Saved"), "saved variable write failure hidden");
    Check(lua_gettop(s_context)==0, "serialization failure stack");
    Check(FrameScript_LoadVariables("serialized.lua", &status), "serialized variable reload");
    Run("assert(Saved.value==73 and Saved.text=='a\\000b\\n\\\"' and Saved.array[2]==2 and Saved.self==nil and Saved.bad==nil)");
}
int main() {
    try {
        bool noOwner = false;
        try { FrameScript_Initialize(0); } catch (const std::logic_error&) { noOwner = true; }
        Check(noOwner, "unowned FrameScript initialization accepted");
        auto directory = std::filesystem::temp_directory_path()/"wowdecomp-framescript-fixture";
        std::filesystem::create_directories(directory);
        struct Cleanup { std::filesystem::path path; ~Cleanup() { std::filesystem::remove_all(path); } } cleanup{directory};
        { std::ofstream file(directory/"fixture.lua"); file << "fromFile=37"; }
        { std::ofstream file(directory/"saved.lua"); file << "Saved={value=73,text='hello'}"; }
        { std::ofstream file(directory/"empty.lua"); }
        for (int cycle=0;cycle<3;++cycle) for (int profile=0;profile<2;++profile) {
            {
                FileQueue queue;
                auto read = [&](SourceKind kind, const std::string& path, std::vector<uint8_t>& bytes) {
                    Check(kind == SourceKind::SavedVariables || path != "saved.lua", "source namespaces mixed");
                    if (path == "throw.lua") throw std::runtime_error("authored provider failure");
                    if (path == "oversize.lua") { bytes.resize(8*1024*1024u+1); return true; }
                    std::string leaf = path.substr(path.find_last_of('\\')+1);
                    bool success = false;
                    auto ticket = queue.ReadHost((directory/leaf).u8string(),0,8*1024*1024u+1,[&](const ReadResult& result) {
                        success = result.status == ReadStatus::Success;
                        bytes = result.file.bytes;
                    });
                    Check(queue.Wait(ticket,std::chrono::seconds(5)), "retained source read timeout");
                    return success;
                };
                Runtime runtime(read,profile);
                Check(FrameScript_GetContext() && lua_gettop(s_context)==0, "original initialization stack");
                bool duplicate=false;
                try { Runtime other(read); } catch (const std::logic_error&) { duplicate=true; }
                Check(duplicate,"second global VM owner accepted");
                LibraryCheck(); SecurityCheck(); EventCheck(); FileCheck(directory);
                bool foreign=false;
                std::thread thread([&] { try { FrameScript_Execute("x=1","@foreign",nullptr); } catch (const std::logic_error&) { foreign=true; } });
                thread.join(); Check(foreign,"foreign VM execution accepted");
                FrameScript_Flush();
                Run("assert(Saved==nil and fromFile==nil and strtrim(' x ')=='x')");
                FrameScript_MemoryCleanup();
                Check(queue.Pending()==0,"source callback survived read");
            }
            Check(s_context==nullptr && !OsLiveThreadCount(),"FrameScript/file-worker teardown");
            std::cout << "FrameScript cycle " << cycle << " profile " << profile << " passed\n";
        }
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
