#include "Engine/Source/Gxu/Portable/FontFace.h"
#include "Engine/Source/Storm/H/storm.h"
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
}
int main(int argc,char**argv) {
    try {
        Require(argc==2,"Font fixture path required"); StormRtlInitialize();
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
