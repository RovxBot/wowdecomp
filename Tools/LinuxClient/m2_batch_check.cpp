// Compile the actual adapted M2 dispatcher, merged submission, DrawBatch and
// SetBatchVertices. Recording services isolate binding ownership without game
// data or a GPU; the unchanged donor dispatcher is the negative control.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

static void check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "M2 batch check failed at line %d\n", line);
        std::abort();
    }
}
#define require(value) check((value), __LINE__)
#define DebugLog(...) ((void)0)

enum { GxXform_View, GxXform_Projection, GxXform_Tex0, GxXform_Tex1, GxXform_World,
       GxRs_DepthFunc, GxRs_PolygonOffset, GxRs_Lighting, GxRs_Fog, GxRs_MatSpecularExp,
       GxCmp_LEqual, GxApi_OpenGl, GxPrim_Triangles, GxSh_Vertex, GxVBF_PBNT2 };
using M2PASS = int;
struct C44Matrix {
    float a0=1, b0=0, c0=0, d0=0, a1=0, b1=1, c1=0, d1=0,
          a2=0, b2=0, c2=1, d2=0, a3=0, b3=0, c3=0, d3=1;
    float Determinant() const { return 1; }
    C44Matrix Inverse(float) const { return *this; }
    C44Matrix Transpose() const { return *this; }
};
struct CGxMatrixStack {
    enum { F_Identity=1 };
    int m_dirty=0, m_level=0;
    unsigned m_flags[8]{};
    C44Matrix m_mtx[8];
};
struct CGxBuf { uint32_t bytes, m_index=0; };
struct CGxBatch { unsigned m_primType, m_start, m_count, m_minIndex, m_maxIndex; };
struct Submission { CGxBuf* indices; CGxBuf* vertices; unsigned first, count; };
struct Device {
    CGxMatrixStack m_xforms[5];
    C44Matrix m_projNative;
    CGxBuf *index=nullptr, *vertex=nullptr;
    bool skinAvailable=true;
    std::vector<Submission> draws;
    float constants[4096]{};
    void RsPush() {}
    void RsPop() {}
    void XformPush(unsigned xf) { ++m_xforms[xf].m_level; }
    void XformSetView(const C44Matrix&) {}
    int DeviceApi() const { return -1; }
    void PrimIndexPtr(CGxBuf* b) { index=b; }
    float* ShaderConstantsLock(int) { return constants; }
    void ShaderConstantsUnlock(int, unsigned, unsigned) {}
    void GpuSkinMesh(int) {}
    int GpuSkinMesh() const { return 0; }
    void GpuSkinInfluences(unsigned) {}
    int GpuSkinBind(const C44Matrix* b, unsigned) { return !b || skinAvailable; }
    void Draw(CGxBatch* b, int) {
        require(index && vertex);
        draws.push_back({index, vertex, b->m_start + (index->m_index >> 1), b->m_count});
    }
};
static Device* g_theGxDevicePtr;
static void GxPrimVertexPtr(CGxBuf* b, int) { g_theGxDevicePtr->vertex=b; }
static void GxRsSet(int, float) {}
static C44Matrix s_identity;
struct CShaderEffect {
    inline static bool s_enableShaders=false;
    void SetCurrent() {}
    static void SetShaders(unsigned, unsigned) {}
    static void UpdateProjMatrix() {}
};
struct CShadowCache { static void SetShadowMapGenericGlobal() {} };
struct CM2Lighting {};
struct M2Material { unsigned m_flags=0, m_blendMode=0; };
struct M2Data {
    struct { unsigned m_count=2; } m_bones;
    M2Material m_materials[1];
    unsigned m_boneCombos[1]{};
};
struct CM2Shared;
struct CM2Cache {
    unsigned m_flags=0;
    void LinkToSharedUpdateList(CM2Shared*) {}
};
struct CM2Shared {
    M2Data* m_data;
    CM2Cache* m_cache;
    const char* m_fileNameWithoutPath="fixture";
    CGxBuf *index, *vertex;
    unsigned indexBinds=0, vertexBinds=0;
    int SetIndices() { ++indexBinds; g_theGxDevicePtr->PrimIndexPtr(index); return 1; }
    void SetVertices(unsigned) { ++vertexBinds; g_theGxDevicePtr->vertex=vertex; }
};
struct M2SkinSection {
    unsigned m_indexStart=0, m_indexCount=0, m_vertexStart=0, m_vertexCount=1,
             m_boneComboIndex=0, m_boneCount=1, m_boneInfluences=1;
};
struct M2Batch { unsigned m_materialIndex=0, m_flags=0, m_shader=0; };
struct CM2Model {
    CM2Shared* m_shared;
    CM2Lighting* m_currentLighting=nullptr;
    unsigned m_fFlags=0, vertexBinds=0;
    CGxBuf *index, *vertex;
    C44Matrix m_boneMatrices[1];
    void SetIndices() { g_theGxDevicePtr->PrimIndexPtr(index); }
    void SetVertices(unsigned flags, M2SkinSection*, bool) { ++vertexBinds; g_theGxDevicePtr->vertex=(flags & 4) ? vertex : m_shared->vertex; }
    void SetVerticesMultiSample(unsigned flags, M2SkinSection*) { ++vertexBinds; g_theGxDevicePtr->vertex=(flags & 4) ? vertex : m_shared->vertex; }
};
struct M2Element {
    int m_type=0;
    CM2Model* m_model=nullptr;
    M2Batch* m_batch=nullptr;
    M2SkinSection* m_skinSection=nullptr;
    CShaderEffect* m_effect=nullptr;
    unsigned m_flags=0, m_vertexPermute=0, m_pixelPermute=0, m_index=0, m_count=1;
};
struct CM2SceneRender {
    C44Matrix m_matrix;
    CM2Cache* m_cache;
    M2Data* m_data=nullptr;
    M2PASS m_pass=0;
    M2Element *m_curElement=nullptr, *m_prevElement=nullptr;
    int m_curType=-1, m_prevType=-1;
    CM2Model *m_curModel=nullptr, *m_prevModel=nullptr;
    CM2Shared *m_curShared=nullptr, *m_prevShared=nullptr;
    CM2Lighting *m_curLighting=nullptr, *m_prevLighting=nullptr;
    int m_curLocalLighting=0, m_prevLocalLighting=0;
    unsigned m_curFogMode=0, m_prevFogMode=0;
    M2Batch *m_curBatch=nullptr, *m_prevBatch=nullptr;
    M2SkinSection *m_curSkinSection=nullptr, *m_prevSkinSection=nullptr;
    M2Material *m_curMaterial=nullptr, *m_prevMaterial=nullptr;
    M2Material m_defaultMaterial;
    void SetupLighting() {}
    void SetupMaterial() {}
    void SetupTextures() {}
    void DrawBatchProj() { std::abort(); }
    void DrawBatchDoodad(M2Element*, uint32_t*) { std::abort(); }
    void DrawRibbon() { std::abort(); }
    unsigned DrawParticle(unsigned, M2Element*, uint32_t*, unsigned) { std::abort(); }
    void DrawCallback() { std::abort(); }
    void SetBatchVertices(int32_t);
    void DrawBatch();
    void Draw(M2PASS, M2Element*, uint32_t*, uint32_t);
    void DrawBeforeFix(M2PASS, M2Element*, uint32_t*, uint32_t);
};
static void ForgetDoodadSetup() {}
struct CharacterMesh;
static CharacterMesh* fixtureMesh=nullptr;
static CharacterMesh* CharacterMeshFor(CM2Shared*, M2Element*, uint32_t*, uint32_t, uint32_t) { return fixtureMesh; }
// Group discovery is outside this ownership test. It supplies the seven-element
// run observed in the live portrait; all submissions and cache updates are real.
static unsigned CharacterRun(M2Element* e, uint32_t* order, uint32_t, uint32_t slot) { return e[order[slot]].m_count; }
#include "RovNativeM2Batch.inc"
#include "RovNativeM2BatchBefore.inc"

static unsigned invalidRanges(const Device& device) {
    unsigned bad=0;
    for (const auto& d : device.draws) {
        if ((uint64_t(d.first)+d.count)*sizeof(uint16_t) > d.indices->bytes) ++bad;
    }
    return bad;
}

static void scenario(bool before, bool merge, bool skin, bool shaders, bool sameSection, bool optimized) {
    Device device;
    device.skinAvailable=skin;
    g_theGxDevicePtr=&device;
    CShaderEffect::s_enableShaders=shaders;
    CM2Cache cache;
    M2Data data;
    CGxBuf fullIndex{33990}, fullVertex{48*5000}, compactIndex{6516}, compactVertex{48*851};
    CGxBuf optIndex{33990}, optVertex{48*5000};
    CM2Shared shared{&data, &cache, "fixture", &fullIndex, &fullVertex};
    CM2Model model{&shared, nullptr, 0, 0, &optIndex, &optVertex};
    M2Batch material;
    CShaderEffect effect;
    M2SkinSection sections[9];
    M2Element elements[9];
    uint32_t order[9];
    unsigned total=0;
    for (unsigned i=0; i<9; ++i) {
        sections[i].m_indexStart=total;
        sections[i].m_indexCount=i<6 ? 450 : (i==6 ? 558 : (i==7 ? 138 : 246));
        total+=sections[i].m_indexCount;
        elements[i].m_model=&model;
        elements[i].m_batch=&material;
        elements[i].m_skinSection=&sections[i];
        elements[i].m_effect=&effect;
        order[i]=i;
    }
    sections[7].m_indexStart=9924;
    sections[8].m_indexStart=10062;
    elements[0].m_count=7;
    if (sameSection) elements[7].m_skinSection=&sections[6];
    if (optimized) elements[7].m_flags=4;
    CharacterMesh mesh{};
    mesh.shared=&shared;
    mesh.sectionCount=7;
    mesh.verts=&compactVertex;
    mesh.indices=&compactIndex;
    mesh.indexCount=3258;
    mesh.vertexCount=851;
    fixtureMesh=merge ? &mesh : nullptr;
    CM2SceneRender render{};
    render.m_cache=&cache;
    if (before) render.DrawBeforeFix(0, elements, order, 9);
    else render.Draw(0, elements, order, 9);
    unsigned submitted=0;
    for (const auto& draw : device.draws) submitted+=draw.count;
    require(submitted==3258 + (sameSection ? 558u : 138u) + 246);
    const bool merged=merge && skin;
    require(device.draws.size()==(merged ? 3 : 9));
    if (merged) {
        require(device.draws[0].indices==&compactIndex && device.draws[0].vertices==&compactVertex);
        require(device.draws[0].first==0 && device.draws[0].count==3258);
    }
    const auto& next=device.draws[device.draws.size()-2];
    require(next.count==138 || sameSection);
    const auto& last=device.draws.back();
    require(last.count==246 && last.first==10062);
    if (before && merged && !optimized) {
        require(next.indices==&compactIndex && last.indices==&compactIndex);
        require(invalidRanges(device)==(sameSection ? 1u : 2u));
        if (sameSection) require(next.vertices==&compactVertex); // Also exposes skipped vertex restoration.
    } else {
        require(next.indices==(optimized ? &optIndex : &fullIndex));
        require(last.indices==&fullIndex && last.vertices==&fullVertex);
        require(next.vertices==(optimized && !shaders ? &optVertex : &fullVertex));
        require(invalidRanges(device)==0);
    }
}

int main() {
    // Exact observed 9924/138 against 6516 bytes, then another required batch.
    scenario(true, true, true, false, false, false);
    scenario(false, true, true, false, false, false);
    // Both vertex restoration predicates and shared/optimized index ownership.
    for (bool shaders : {false, true}) {
        scenario(false, true, true, shaders, true, false);
        scenario(false, true, true, shaders, false, true);
        scenario(false, false, true, shaders, false, false); // Mesh unavailable.
        scenario(false, true, false, shaders, false, false); // GPU skin unavailable.
    }
    scenario(true, true, true, true, true, false);
    std::puts("m2-batch=passed: actual-source stale-binding reproducer, required draws, index/vertex ownership and fallback");
}
