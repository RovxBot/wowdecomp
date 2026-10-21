#ifndef ENGINE_SOURCE_FRAME_CRENDERBATCH_H
#define ENGINE_SOURCE_FRAME_CRENDERBATCH_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/Frame/CSimpleBatchedMesh.h"
#include "Engine/Source/Gx/Types.h"
#include "Engine/Source/Storm/H/stpl.h"
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
#include "Engine/Source/Services/Types.h"
namespace Frame { namespace Native { class RenderScope; }}
#endif

class C2Vector;
class C3Vector;
class CGxShader;
struct CGxStringBatch;
class CImVector;
class CSimpleFontString;
class CSimpleTexture;
class CTexture;

struct RENDERCALLBACKNODE : public TSLinkedNode<RENDERCALLBACKNODE> {
    /* 0008 */ void (*m_callback)(void* param);
    /* 000C */ void* m_param;
};

class CRenderBatch {
    public:
    /* 0004 */ uint32_t m_count;
    /* 0008 */ TSGrowableArray<CSimpleBatchedMesh> m_texturelist;
    /* 0018 */ CGxStringBatch* m_stringbatch;
    /* 001C */ TSList<RENDERCALLBACKNODE, TSGetLink<RENDERCALLBACKNODE> > m_callbacks;
    /* 0028 */ TSLink<CRenderBatch> m_renderLink;

    virtual ~CRenderBatch();

    CRenderBatch();
    void Clear();
    void Finish();
    void Queue(CTexture* texture, EGxBlend alphaMode, int32_t posCount, const C3Vector* position, const C2Vector* texCoord, int32_t colorCount, const CImVector* color, int32_t idxCount, const uint16_t* indices, CGxShader* shader);
    void QueueCallback(void (*callback)(void*), void* param);
    void QueueFontString(CSimpleFontString* string);
    void QueueTexture(CSimpleTexture* texture);
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
    // Retain both handles: the original Services text block borrows its font.
    void NativeQueueText(HTEXTFONT font, HTEXTBLOCK text);
    void NativeCheck(bool mutation = true) const;
    CRenderBatch(const CRenderBatch&) = delete;
    CRenderBatch& operator=(const CRenderBatch&) = delete;
private:
    friend class Frame::Native::RenderScope;
    void NativeBeginDraw();
    void NativeEndDraw();
    struct NativeState;
    NativeState* m_native = nullptr;
    void NativeConstruct();
    void NativeDestroy();
    void NativeRelease();
    void NativeQueue(CTexture*, EGxBlend, int32_t, const C3Vector*, const C2Vector*, int32_t, const CImVector*, int32_t, const uint16_t*, CGxShader*);
#endif
};

#endif
