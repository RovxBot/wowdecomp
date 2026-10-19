#include "Engine/Source/Gxu/Portable/FontFace.h"
#include "Engine/Source/Storm/H/storm.h"
#include <freetype/internal/ftstream.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <thread>
using namespace Gxu::Native;
namespace {
void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class E,class F> void Reject(F f) {
    bool rejected=false;try {f();} catch(const E&){rejected=true;}
    Require(rejected,"Invalid font operation accepted");
}
void StreamChecks() {
    FT_Byte big[]={0xDE,0xAD,0xBE,0xEF}, little[]={0xEF,0xBE,0xAD,0xDE};
    FT_Byte negative[]={0xFF,0xDF}, offset[]={0x80,0x00,0x00};
    FT_Byte* p=big; Require(NEXT_Long(p)==-559038737L && p==big+4,"Signed big-endian font word changed");
    p=big; Require(NEXT_ULong(p)==0xDEADBEEFUL,"Unsigned font word sign-extended to host long width");
    p=little; Require(NEXT_LongLE(p)==-559038737L && p==little+4,"Signed little-endian font word changed");
    p=little; Require(NEXT_ULongLE(p)==0xDEADBEEFUL,"Unsigned little-endian font word changed");
    p=negative; Require(NEXT_Short(p)==-33,"Signed font short changed");
    p=offset; Require(NEXT_Offset(p)==-8388608L,"Signed 24-bit font offset changed");
    FT_StreamRec_ record{}; FT_Stream stream=&record;
    record.cursor=big; record.limit=big+4;
    Require(GET_ULong()==0xDEADBEEFUL,"Framed unsigned font word changed");
    record.cursor=little; record.limit=little+4;
    Require(GET_LongLE()==-559038737L,"Framed little-endian font word was truncated");
    record.base=big;record.size=4;record.pos=0;
    FT_Error error=0;FT_ULong value=0;
    Require(!READ_ULong(value) && value==0xDEADBEEFUL,"Read unsigned font word changed");
}
}
int main(int argc,char**argv) {
    try {
        Require(argc==2,"Font fixture path required"); StormRtlInitialize(); StreamChecks();
        std::ifstream input(argv[1],std::ios::binary);
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
        Require(!bytes.empty(),"Font fixture unreadable");
        for(int cycle=0;cycle<3;++cycle) {
            FontFace face(bytes);Require(face.Family()=="Native Geometric Fixture","Unexpected font family");
            for(uint32_t size:{8u,16u,32u}) {
                auto glyph=face.Raster('A',size);
                Require(glyph.width>0 && glyph.height>0 && glyph.advance>0 && glyph.coverage.size()==size_t(glyph.width)*glyph.height,"Invalid scalable glyph metrics");
                Require(std::any_of(glyph.coverage.begin(),glyph.coverage.end(),[](uint8_t c){return c>0&&c<255;}),"Grayscale raster has no antialias coverage");
                auto mono=face.Raster('A',size,true);
                Require(!mono.coverage.empty() && std::all_of(mono.coverage.begin(),mono.coverage.end(),[](uint8_t c){return c==0||c==255;}),"Monochrome raster not expanded to binary coverage");
                auto space=face.Raster(' ',size);
                Require(space.coverage.empty() && space.advance>0,"Whitespace lost its advance");
                auto curve=face.Raster('Q',size);
                Require(curve.bearing<0 && curve.top<int32_t(curve.height) && !curve.coverage.empty(),"Curve lost negative bearing or descender");
                auto composite=face.Raster(0x100,size);
                Require(composite.width==glyph.width && composite.height==glyph.height && composite.advance==glyph.advance,"Unicode transformed composite has inconsistent metrics");
                Require(std::any_of(composite.coverage.begin(),composite.coverage.end(),[](uint8_t c){return c!=0;}),"Composite raster is blank");
                Require(face.Step('A','V',size)<=glyph.advance,"Negative original kerning increased advance");
            }
            Require(face.Step('A','V',16)<face.Raster('A',16).advance,"Synthetic AV kerning was ignored");
            Reject<std::out_of_range>([&]{face.Raster('B',16);});
            Reject<std::out_of_range>([&]{face.Raster('A',0);});
            Reject<std::out_of_range>([&]{face.Raster('A',33);});
            Reject<std::invalid_argument>([&]{face.Raster(0xD800,16);});
            Reject<std::invalid_argument>([&]{face.Raster(0x110000,16);});
            Reject<std::invalid_argument>([]{FontFace empty({});});
            Reject<std::runtime_error>([]{FontFace invalid({0,1,0,0,0,0,0,0});});
            bool rejected=false;std::thread wrong([&]{try{face.Raster('A',16);}catch(const std::logic_error&){rejected=true;}});
            wrong.join();Require(rejected,"Foreign-thread glyph rendering accepted");
            std::cout<<"native-font-memory-raster-mono-gray-space-kerning-bounds-affinity-cycle="<<cycle+1<<";passed\n";
        }
        StormRtlDestroy(); return 0;
    }catch(const std::exception&e){std::cerr<<"Native font checks failed: "<<e.what()<<'\n';return 1;}
}
