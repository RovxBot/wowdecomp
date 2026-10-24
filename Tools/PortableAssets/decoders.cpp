#include "Engine/Source/BlizzardCore/Portable/Assets.h"
#include <algorithm>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <new>

using namespace Blizzard::Assets;
void Check(bool value, const char* message) { if (!value) throw Error(message); }
void Fail(const std::function<void()>& fn) {
    bool rejected = false;
    try { fn(); } catch (const std::exception&) { rejected = true; }
    Check(rejected, "Invalid asset/relative extent was accepted");
}
void U32(Bytes& b, size_t offset, uint32_t v) {
    if (b.size() < offset + 4) b.resize(offset + 4);
    for (size_t i = 0; i < 4; ++i) b[offset + i] = static_cast<uint8_t>(v >> (i * 8));
}
void Float(Bytes& b, size_t offset, float value) { uint32_t bits; std::memcpy(&bits, &value, 4); U32(b, offset, bits); }
Bytes Chunk(const char* name, Bytes b) {
    Bytes out{uint8_t(name[3]),uint8_t(name[2]),uint8_t(name[1]),uint8_t(name[0])};
    U32(out, 4, static_cast<uint32_t>(b.size())); out.insert(out.end(), b.begin(), b.end()); return out;
}
Bytes Join(Bytes a, const Bytes& b) { a.insert(a.end(), b.begin(), b.end()); return a; }
Bytes Version(uint32_t n) { Bytes v; U32(v, 0, n); return Chunk("MVER", v); }
Bytes Blp(unsigned encoding, unsigned alpha, unsigned format, Bytes pixel) {
    Bytes b(encoding == 1 ? 1172 : 148);
    U32(b, 0, 0x32504c42); U32(b, 4, 1); b[8] = encoding; b[9] = alpha; b[10] = format;
    U32(b, 12, 1); U32(b, 16, 1); U32(b, 20, static_cast<uint32_t>(b.size())); U32(b, 84, static_cast<uint32_t>(pixel.size()));
    if (encoding == 1) { b[148+4] = 30; b[148+5] = 20; b[148+6] = 10; }
    return Join(b, pixel);
}
int main() {
    try {
        Bytes dbc(20 + 24 + 5);
        U32(dbc, 0, 0x43424457); U32(dbc, 4, 2); U32(dbc, 8, 3); U32(dbc, 12, 12); U32(dbc, 16, 5);
        U32(dbc, 20, 12340); U32(dbc, 24, 1); Float(dbc, 28, -3.5f);
        U32(dbc, 32, 7); U32(dbc, 36, 0); Float(dbc, 40, 1.25f);
        dbc[45]='a'; dbc[46]='b'; dbc[47]='c';
        Dbc table(dbc);
        Check(table.Rows()==2 && table.Fields()==3 && table.UInt(0,0)==12340 && table.String(0,1)=="abc" && table.Float(0,2)==-3.5f, "DBC field/string/float decode failed");
        Fail([&] { table.UInt(2,0); }); Fail([&] { table.String(0,0); });
        auto bad = dbc; U32(bad,12,4); Fail([&] { Dbc invalid(bad); });
        for (unsigned alpha : {0u,1u,4u,8u}) {
            Bytes pixel{1}; if (alpha) pixel.push_back(alpha==1 ? 1 : alpha==4 ? 5 : 128);
            auto texture = DecodeBlp(Blp(1,alpha,8,pixel));
            unsigned expectedAlpha = alpha==0 || alpha==1 ? 255 : alpha==4 ? 85 : 128;
            Check(texture[0].rgba==Bytes({10,20,30,static_cast<uint8_t>(expectedAlpha)}), "BLP palette/alpha decode failed");
        }
        Check(DecodeBlp(Blp(3,8,2,Bytes{30,20,10,40}))[0].rgba==Bytes({10,20,30,40}), "BLP raw channel decode failed");
        // DXT blocks still occupy 4x4 pixels for a 1x1 mip; edge writes are clipped.
        Bytes dxt1{0,0xf8,0xe0,7,0,0,0,0}; // red / green, first index red
        Check(DecodeBlp(Blp(2,0,0,dxt1))[0].rgba==Bytes({255,0,0,255}), "DXT1 decode failed");
        auto dxt3 = Join(Bytes(8,0x55),dxt1);
        Check(DecodeBlp(Blp(2,8,1,dxt3))[0].rgba==Bytes({255,0,0,85}), "DXT3 alpha failed");
        auto dxt5 = Join(Bytes{200,100,2,0,0,0,0,0},dxt1);
        Check(DecodeBlp(Blp(2,8,7,dxt5))[0].rgba==Bytes({255,0,0,185}), "DXT5 interpolation failed");
        // Original rectangular Glue DXT tails can store fewer blocks than a
        // complete logical mip. Base-only UI reads validate but do not decode
        // those tails; explicit full-chain reads must still fail visibly.
        for (unsigned format : {0u,1u,7u}) {
            const auto& block = format == 0 ? dxt1 : format == 1 ? dxt3 : dxt5;
            auto rectangular = [&](bool legacy) {
                Bytes b(148); U32(b,0,0x32504c42); U32(b,4,1);
                b[8]=2; b[9]=format ? 8 : 0; b[10]=format; b[11]=1;
                U32(b,12,32); U32(b,16,8);
                for (unsigned level=0,w=32,h=8;level<6;++level,w=std::max(1u,w/2),h=std::max(1u,h/2)) {
                    size_t blocks=legacy ? std::max(1u,w*h/16) : ((w+3)/4)*((h+3)/4);
                    U32(b,20+level*4,uint32_t(b.size())); U32(b,84+level*4,uint32_t(blocks*block.size()));
                    for(size_t i=0;i<blocks;++i)b.insert(b.end(),block.begin(),block.end());
                }
                return b;
            };
            auto full=rectangular(false), thin=rectangular(true);
            Check(DecodeBlp(full).size()==6,"Complete rectangular DXT chain rejected");
            Fail([&]{DecodeBlp(thin);});
            const auto base=DecodeBlp(thin,true);
            Check(base.size()==1 && base[0].width==32 && base[0].height==8 && base[0].rgba==DecodeBlp(full)[0].rgba,
                  "Base-only DXT read changed complete base pixels or fabricated tail pixels");
            auto badTail=thin; U32(badTail,84+2*4,uint32_t(block.size()-1)); Fail([&]{DecodeBlp(badTail,true);});
            badTail=thin; U32(badTail,20+5*4,uint32_t(thin.size())); Fail([&]{DecodeBlp(badTail,true);});
            badTail=thin; badTail.pop_back(); Fail([&]{DecodeBlp(badTail,true);});
            badTail=thin; badTail[11]=0; Fail([&]{DecodeBlp(badTail,true);});
            badTail=thin; U32(badTail,84+1*4,uint32_t(block.size())); Fail([&]{DecodeBlp(badTail,true);});
            // The original tooltip border uses a single block at 16x2.
            // Verify both axis orientations, exact base pixels, and strict
            // requested-mip rejection; never invent the missing tail pixels.
            for (bool transpose : {false, true}) {
                auto tooltip = [&](bool complete) {
                    Bytes b(148); U32(b,0,0x32504c42); U32(b,4,1);
                    b[8]=2; b[9]=format ? 8 : 0; b[10]=format; b[11]=1;
                    unsigned w=transpose ? 16 : 128,h=transpose ? 128 : 16;
                    U32(b,12,w); U32(b,16,h);
                    for(unsigned level=0;level<8;++level,w=std::max(1u,w/2),h=std::max(1u,h/2)) {
                        size_t count=complete || std::min(w,h)>=4 ? ((w+3)/4)*((h+3)/4) : 1;
                        U32(b,20+level*4,uint32_t(b.size())); U32(b,84+level*4,uint32_t(count*block.size()));
                        for(size_t i=0;i<count;++i)b.insert(b.end(),block.begin(),block.end());
                    }
                    return b;
                };
                auto tail=tooltip(false),complete=tooltip(true);
                const auto baseOnly=DecodeBlp(tail,true);
                Check(baseOnly.size()==1 && baseOnly[0].rgba==DecodeBlp(complete)[0].rgba,
                      "Tooltip single-block tail changed base pixels or fabricated mip pixels");
                Fail([&]{DecodeBlp(tail);});
                auto bad=tail; U32(bad,84+3*4,uint32_t(block.size()-1)); Fail([&]{DecodeBlp(bad,true);});
                bad=tail; U32(bad,84+3*4,uint32_t(block.size()*3)); Fail([&]{DecodeBlp(bad,true);});
                bad=tail; U32(bad,84+2*4,uint32_t(block.size())); Fail([&]{DecodeBlp(bad,true);});
                bad=tail; U32(bad,84,uint32_t(block.size())); Fail([&]{DecodeBlp(bad,true);});
                bad=tail; bad.pop_back(); Fail([&]{DecodeBlp(bad,true);});
                bad=tail; bad[11]=0; Fail([&]{DecodeBlp(bad,true);});
                bad=tail; U32(bad,transpose ? 16 : 12,127); Fail([&]{DecodeBlp(bad,true);});
            }
        }
        auto texture = Blp(1,8,8,Bytes{1,128}); texture.pop_back(); Fail([&] { DecodeBlp(texture); });
        Bytes model(0x130 + 48);
        U32(model,0,0x3032444d); U32(model,4,264); U32(model,0x3c,1); U32(model,0x40,0x130);
        Float(model,0x130,1.5f); Float(model,0x130+4,-2); Float(model,0x130+8,3.25f);
        model[0x130+12]=255; Float(model,0x130+20,1); Float(model,0x130+32,0.5f);
        Model decoded(model);
        Check(decoded.VertexCount()==1 && decoded.Vertices()[0].position.z==3.25f && decoded.Vertices()[0].weights[0]==255 && decoded.Vertices()[0].uv[0]==0.5f, "M2 vertex decode failed");
        bad=model; U32(bad,0x40,0xfffffff0); Fail([&] { Model invalid(bad); });
        bad=model; U32(bad,0x130,0x7fc00000); Fail([&] { Model invalid(bad); });
        Bytes skin(60); U32(skin,0,0x4e494b53); U32(skin,4,3); U32(skin,8,48); U32(skin,12,3); U32(skin,16,54);
        skin[48]=2; skin[50]=0; skin[52]=1; skin[54]=0; skin[56]=1; skin[58]=2;
        Check(DecodeSkin(skin,3)==std::vector<uint16_t>({2,0,1}), "SKIN profile vertex/triangle lookup failed");
        bad=skin; bad[58]=3; Fail([&] { DecodeSkin(bad,3); });
        Fail([&] { DecodeSkin(skin,2); });
        OsNative::ModelArena arena(65536);
        auto anchor = new (arena.Allocate(sizeof(OsNative::RelativeArray),4)) OsNative::RelativeArray{};
        auto payload = arena.Allocate(32,16); arena.Encode(anchor,payload,8,4);
        Check(arena.Resolve(anchor,4)==payload && anchor->offset>0, "Positive relative encoding failed");
        auto later = new (arena.Allocate(sizeof(OsNative::RelativeArray),4)) OsNative::RelativeArray{};
        arena.Encode(later,payload,8,4); Check(later->offset<0 && arena.Resolve(later,4)==payload, "Negative relative encoding failed");
        Fail([&] { arena.Encode(anchor,model.data(),1,4); });
        Fail([&] { arena.Encode(anchor,payload,UINT32_MAX,std::numeric_limits<size_t>::max()); });
        Fail([&] { arena.Allocate(65536,16); }); Fail([&] { arena.Allocate(4,3); });
        anchor->offset=INT32_MAX; Fail([&] { arena.Resolve(anchor,4); });
        arena.Encode(anchor,nullptr,0,4); Check(!arena.Resolve(anchor,4), "Empty relative array failed");
        Bytes positions(36), normals(36), indices{0,0,1,0,2,0}; Float(positions,12,1); Float(positions,28,1);
        auto geometry=Join(Join(Join(Chunk("MOVT",positions),Chunk("MONR",normals)),Chunk("MOVI",indices)),Chunk("MOPY",Bytes{0,0}));
        auto wmo=Join(Version(17),Chunk("MOGP",Join(Bytes(68),geometry)));
        auto group=DecodeWmo(wmo);
        Check(group.vertices.size()==3 && group.indices.size()==3 && group.vertices[1].x==1, "WMO geometry decode failed");
        auto root=Join(Join(Join(Version(17),Chunk("MOHD",Bytes(64))),Chunk("MOGI",Bytes())),Chunk("MOMT",Bytes()));
        Check(DecodeWmo(root).groups==0, "WMO root decode failed");
        Bytes tileFlags(64*64*8); U32(tileFlags,(48*64+32)*8,1);
        auto wdt=Join(Join(Version(18),Chunk("MPHD",Bytes(32))),Chunk("MAIN",tileFlags));
        auto definition=DecodeWdt(wdt);
        Check(definition.tiles==std::vector<std::pair<uint32_t,uint32_t>>{{32,48}}, "WDT tile selection failed");
        Bytes header(128), heights(145*4); U32(header,4,2); U32(header,8,3); U32(header,20,136); U32(header,0x34,12);
        Float(header,0x70,100); Float(heights,0,1.25f);
        auto adt=Join(Join(Version(18),Chunk("MHDR",Bytes(64))),Chunk("MCNK",Join(header,Chunk("MCVT",heights))));
        auto terrain=DecodeAdt(adt);
        Check(terrain.size()==1 && terrain[0].x==2 && terrain[0].y==3 && terrain[0].area==12 && terrain[0].heights[0]==101.25f, "ADT absolute height decode failed");
        for (const auto& fixture : {dbc, model, wmo, wdt, adt}) {
            for (size_t length : {size_t(0),size_t(3),size_t(8),fixture.size()-1}) {
                Bytes truncated(fixture.begin(),fixture.begin()+length);
                Fail([&] {
                    if (fixture==dbc) { Dbc value(truncated); }
                    else if (fixture==model) { Model value(truncated); }
                    else if (fixture==wmo) DecodeWmo(truncated);
                    else if (fixture==wdt) DecodeWdt(truncated);
                    else DecodeAdt(truncated);
                });
            }
        }
        std::cout << "DBC, BLP palette/raw/DXT1/3/5, M2 owner arena/vertices, WMO triangles, WDT tiles and ADT heights passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
