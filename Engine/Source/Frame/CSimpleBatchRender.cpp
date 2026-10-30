#include <cstddef>
#include <cstdint>
#include <malloc.h>
#include "Engine/Source/Frame/CSimpleRender.h"
#include "Engine/Source/Frame/CRenderBatch.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/CGxDevice/CGxShader.h"
#include "Engine/Source/Gx/CGxVertexPCT.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Gxu/GxuFont.h"
#include "Engine/Source/Services/Texture.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
#include "Engine/Source/Frame/Portable/Render.h"
#include "Engine/Source/Gx/CGxBuf.h"
#include "Engine/Source/Gx/Portable/LegacyDevice.h"
#include <vector>
#include <stdexcept>
namespace {
struct StateScope {
    StateScope() { g_theGxDevicePtr->RsPush(); }
    ~StateScope() { g_theGxDevicePtr->RsPop(); }
};
struct StreamScope {
    CGxBuf *vertex = nullptr, *index = nullptr;
    bool vertexOwned = false, indexOwned = false;
    ~StreamScope() {
        // A failure during allocation/upload must abort retained staging writes.
        auto* device = static_cast<Gx::Native::LegacyDevice*>(g_theGxDevicePtr);
        if (vertexOwned && device->m_bufLocked[GxPoolTarget_Vertex] == vertex)
            device->AbortBufferWrite(vertex);
        if (indexOwned && device->m_bufLocked[GxPoolTarget_Index] == index)
            device->AbortBufferWrite(index);
    }
};
}
#endif

// STATIC: 0x00B47940
CGxShader* CSimpleRender::s_vertexShader[2];

// STATIC: 0x00B47948
int32_t CSimpleRender::s_mergeEnable;

// VERIFY: 86381625 auto
// OFFSET: 0x00482E90
void CSimpleRender::Init() {
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
    Frame::Native::RequireRenderer();
#endif
    g_theGxDevicePtr->ShaderCreate(CSimpleRender::s_vertexShader, GxSh_Vertex, "Shaders\\Vertex", "UI", 2);
}

// VERIFY: ea0ca375 manual
// OFFSET: 0x00482EC0
void CSimpleRender::Destroy() {
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
    Frame::Native::RequireRenderer();
#endif
    for (int32_t i = 0; i < 2; i++) {
        if (CSimpleRender::s_vertexShader[i]) {
            g_theGxDevicePtr->ShaderDestroy(CSimpleRender::s_vertexShader[i]);
        }
    }
}

// VERIFY: d4f7147d manual
// OFFSET: 0x00482F00
int32_t CSimpleRender::CalcBatchCounts(CRenderBatch* batch, uint32_t* vertCounts, uint32_t* idxCounts) {
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
    if (!batch || !vertCounts || !idxCounts) throw std::invalid_argument("Null Frame batch counts");
    batch->NativeCheck(false);
#endif
    CGxTex* textureID = reinterpret_cast<CGxTex*>(-1);
    int32_t alphaMode = GxBlends_Last;
    CGxShader* shader = nullptr;
    int32_t batchIndex = -1;
    uint32_t vertTotal = 0;
    uint32_t idxTotal = 0;

    for (uint32_t i = 0; i < batch->m_texturelist.Count(); i++) {
        CSimpleBatchedMesh* mesh = &batch->m_texturelist[i];
        int32_t changed = 0;

        if (mesh->m_textureID != textureID) {
            textureID = mesh->m_textureID;
            changed = 1;
        }

        int32_t meshAlphaMode = (mesh->m_color && mesh->m_alphaMode < GxBlend_Alpha) ? GxBlend_Alpha : mesh->m_alphaMode;

        if (meshAlphaMode != alphaMode) {
            alphaMode = meshAlphaMode;
            changed = 1;
        }

        if (mesh->m_shader != shader) {
            shader = mesh->m_shader;
            changed = 1;
        }

        if (!CSimpleRender::s_mergeEnable || changed || idxTotal + mesh->m_idxCount >= 0x20000 || vertTotal + mesh->m_posCount >= 0x10000) {
            if (batchIndex > -1) {
                vertCounts[batchIndex] = vertTotal;
                idxCounts[batchIndex] = idxTotal;
            }

            batchIndex++;
            vertTotal = 0;
            idxTotal = 0;
        }

        vertTotal += mesh->m_posCount;
        idxTotal += mesh->m_idxCount;
    }

    if (idxTotal) {
        vertCounts[batchIndex] = vertTotal;
        idxCounts[batchIndex] = idxTotal;
        batchIndex++;
    }

    return batchIndex;
}

// VERIFY: 9b5bcef4 manual
// OFFSET: 0x00484B00
void CSimpleRender::DrawBatch(CRenderBatch* batch) {
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
    Frame::Native::RenderScope renderScope(batch);
#endif
    uint32_t meshCount = batch->m_texturelist.Count();

    if (meshCount) {
        CGxTex* textureID = reinterpret_cast<CGxTex*>(-1);
        int32_t alphaMode = GxBlends_Last;
        CGxShader* shader = nullptr;

#if defined(WOWDECOMP_PORTABLE_RUNTIME)
        StateScope states;
#else
        g_theGxDevicePtr->RsPush();
#endif

        CGxShader* vertexShader = g_theGxDevicePtr->StereoEnabled() ? CSimpleRender::s_vertexShader[1] : CSimpleRender::s_vertexShader[0];

        if (vertexShader && vertexShader->Valid()) {
            C44Matrix viewProj;
            GxRsSet(GxRs_VertexShader, vertexShader);
            GxXformViewProjNativeTranspose(viewProj);
            g_theGxDevicePtr->ShaderConstantsSet(GxSh_Vertex, 0, reinterpret_cast<const float*>(&viewProj), 4);
        } else {
            GxRsSet(GxRs_Lighting, 0);
        }

        GxRsSet(GxRs_Fog, 0);
        GxRsSet(GxRs_DepthTest, 0);
        GxRsSet(GxRs_DepthWrite, 0);

#if defined(WOWDECOMP_PORTABLE_RUNTIME)
        std::vector<uint32_t> vertexCounts(meshCount), indexCounts(meshCount);
        uint32_t* vertCounts = vertexCounts.data();
        uint32_t* idxCounts = indexCounts.data();
#else
        uint32_t* vertCounts = static_cast<uint32_t*>(alloca(sizeof(uint32_t) * meshCount));
        uint32_t* idxCounts = static_cast<uint32_t*>(alloca(sizeof(uint32_t) * meshCount));
#endif
        CSimpleRender::CalcBatchCounts(batch, vertCounts, idxCounts);

        CGxBuf* vertexBuf = nullptr;
        CGxBuf* indexBuf = nullptr;
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
        StreamScope streams;
#endif
        CGxVertexPCT* vertices = nullptr;
        uint16_t* indices = nullptr;
        uint32_t vertTotal = 0;
        int32_t idxTotal = 0;
        int32_t batchIndex = -1;

        for (uint32_t i = 0; i < meshCount; i++) {
            CSimpleBatchedMesh* mesh = &batch->m_texturelist[i];
            int32_t changed = 0;

            if (mesh->m_textureID != textureID) {
                textureID = mesh->m_textureID;
                changed = 1;
            }

            int32_t meshAlphaMode = (mesh->m_color && mesh->m_alphaMode < GxBlend_Alpha) ? GxBlend_Alpha : mesh->m_alphaMode;

            if (meshAlphaMode != alphaMode) {
                alphaMode = meshAlphaMode;
                changed = 1;
            }

            if (mesh->m_shader != shader) {
                shader = mesh->m_shader;
                changed = 1;
            }

            bool newBatch = !CSimpleRender::s_mergeEnable || changed || idxTotal + mesh->m_idxCount >= 0x20000 || vertTotal + mesh->m_posCount >= 0x10000;

            if (newBatch) {
                if (idxTotal > 0) {
                    GxBufUnlock(vertexBuf, 0);
                    GxBufUnlock(indexBuf, 0);
                    GxPrimVertexPtr(vertexBuf, GxVBF_PCT);
                    g_theGxDevicePtr->PrimIndexPtr(indexBuf);

                    CGxBatch gxBatch;
                    gxBatch.m_primType = GxPrim_Triangles;
                    gxBatch.m_start = 0;
                    gxBatch.m_count = idxCounts[batchIndex];
                    gxBatch.m_minIndex = 0;
                    gxBatch.m_maxIndex = vertCounts[batchIndex] - 1;
                    GxDraw(&gxBatch, 1);
                }

                batchIndex++;
                idxTotal = 0;
                vertTotal = 0;

                GxRsSet(GxRs_Texture0, TextureGetGxTex(mesh->m_texture, 1, nullptr));
                GxRsSet(GxRs_BlendingMode, alphaMode);
                GxUpdateAlphaRef(GxRs_AlphaRef);
                GxRsSet(GxRs_PixelShader, shader);
            }

            if (newBatch || !vertices) {
                vertexBuf = GxBufStream(GxPoolTarget_Vertex, sizeof(CGxVertexPCT), vertCounts[batchIndex]);
                indexBuf = GxBufStream(GxPoolTarget_Index, sizeof(uint16_t), idxCounts[batchIndex]);
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
                streams.vertex = vertexBuf;
                streams.index = indexBuf;
#endif
                vertices = reinterpret_cast<CGxVertexPCT*>(GxBufLock(vertexBuf));
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
                streams.vertexOwned = true;
                streams.indexOwned = false;
#endif
                indices = reinterpret_cast<uint16_t*>(GxBufLock(indexBuf));
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
                streams.indexOwned = true;
#endif
            }

            if (!mesh->m_indices) {
                continue;
            }

            for (uint32_t v = 0; v < mesh->m_posCount; v++) {
                vertices->p = mesh->m_position[v];

                if (mesh->m_color) {
                    CImVector color = mesh->m_color[(v * mesh->m_colorCount) >> 2];
                    GxFormatColor(color);
                    vertices->c = color;
                } else {
                    CImVector white;
                    white.b = 0xFF;
                    white.g = 0xFF;
                    white.r = 0xFF;
                    white.a = 0xFF;
                    vertices->c = white;
                }

                if (mesh->m_onAtlas) {
                    vertices->tc.x = mesh->m_atlasScale * mesh->m_texCoord[v].x + mesh->m_atlasOffset.x;
                    vertices->tc.y = mesh->m_atlasScale * mesh->m_texCoord[v].y + mesh->m_atlasOffset.y;
                } else {
                    vertices->tc = mesh->m_texCoord[v];
                }

                vertices++;
            }

            for (uint32_t j = 0; j < mesh->m_idxCount; j++) {
                *indices = vertTotal + mesh->m_indices[j];
                indices++;
            }

            vertTotal += mesh->m_posCount;
            idxTotal += mesh->m_idxCount;
        }

        if (idxTotal > 0) {
            GxBufUnlock(vertexBuf, sizeof(CGxVertexPCT) * vertTotal);
            GxBufUnlock(indexBuf, sizeof(uint16_t) * idxTotal);
            GxPrimVertexPtr(vertexBuf, GxVBF_PCT);
            g_theGxDevicePtr->PrimIndexPtr(indexBuf);

            CGxBatch gxBatch;
            gxBatch.m_primType = GxPrim_Triangles;
            gxBatch.m_start = 0;
            gxBatch.m_count = idxCounts[batchIndex];
            gxBatch.m_minIndex = 0;
            gxBatch.m_maxIndex = vertCounts[batchIndex] - 1;
            GxDraw(&gxBatch, 1);
        }

#if !defined(WOWDECOMP_PORTABLE_RUNTIME)
        g_theGxDevicePtr->RsPop();
#endif
    }

    if (batch->m_stringbatch) {
        GxuFontRenderBatch(batch->m_stringbatch);
    }

    for (RENDERCALLBACKNODE* node = batch->m_callbacks.Head(); !TSList<RENDERCALLBACKNODE, TSGetLink<RENDERCALLBACKNODE> >::IsEnd(node); node = node->RawNext()) {
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
        Frame::Native::RenderCallbackScope callbackScope(node);
#endif
        node->m_callback(node->m_param);
    }
}
