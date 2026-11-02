#ifndef WOWDECOMP_NATIVE_FRAME_HIERARCHY_H
#define WOWDECOMP_NATIVE_FRAME_HIERARCHY_H
#include "Regions.h"
class CSimpleTop;
class CSimpleTitleRegion;
class CSimpleTexture;
class CSimpleScrollFrame;
class CSimpleButton;
class CSimpleEditBox;
class CSimpleSlider;
namespace Frame { namespace Native {
// Frames are owned by their parent/root. Heap and selected pool allocations may
// coexist. Borrow until destruction; XML/Lua factory registration is separate.
void RequireRoot();
using ScrollNotify = void (*)(CSimpleFrame*);
void SetScrollNotify(ScrollNotify);
bool HasScrollSupport();
void NotifyScroll(CSimpleFrame*);
void RequireScriptTextMutation();
// Existing image handles may change during update scripts; region ownership,
// construction/deletion, resize, collection and render remain pinned.
void RequireTextureContentMutation();
void RequireVisibilityMutation(CSimpleFrame*, bool showing);
// Only lazy width invalidation of an existing hidden sibling decoration during
// the selected Button transition. Other layout methods retain their guards.
void RequireRegionWidthMutation(CScriptRegion*);
void RequireSliderInput(CSimpleFrame*);
class SliderValueEdits final {
public:
    ~SliderValueEdits();
    SliderValueEdits(const SliderValueEdits&) = delete;
    SliderValueEdits& operator=(const SliderValueEdits&) = delete;
private:
    friend class ::CSimpleSlider;
    explicit SliderValueEdits(CSimpleFrame*);
    bool active = false;
};
// Only the original scroll-range notification may update qualified existing
// controls while layer traversal is pinned. No tree/layout/deletion capability.
struct ScrollRangeEdits final {
    ScrollRangeEdits();
    ~ScrollRangeEdits();
    ScrollRangeEdits(const ScrollRangeEdits&) = delete;
    ScrollRangeEdits& operator=(const ScrollRangeEdits&) = delete;
};
// Enter only around the original leaf wheel script during guarded dispatch.
// Nested Slider/scroll/visibility/Button callbacks share the bounded state
// admission; hierarchy, layout, input reentry and deletion stay pinned.
class MouseWheelEdits final {
public:
    ~MouseWheelEdits();
    MouseWheelEdits(const MouseWheelEdits&) = delete;
    MouseWheelEdits& operator=(const MouseWheelEdits&) = delete;
private:
    friend class ::CSimpleFrame;
    MouseWheelEdits();
    bool active = false;
};
// Original selected Button PreClick/OnClick/PostClick may change qualified
// existing controls. Other consumers, nested clicks and cursor-exit stay guarded.
class ButtonClickEdits final {
public:
    ~ButtonClickEdits();
    ButtonClickEdits(const ButtonClickEdits&) = delete;
    ButtonClickEdits& operator=(const ButtonClickEdits&) = delete;
private:
    friend class ::CSimpleButton;
    explicit ButtonClickEdits(CSimpleFrame*);
    bool active = false;
};
// Original deferred EditBox OnTextChanged may update existing hints/controls.
class TextChangedEdits final {
public:
    ~TextChangedEdits();
    TextChangedEdits(const TextChangedEdits&) = delete;
    TextChangedEdits& operator=(const TextChangedEdits&) = delete;
private:
    friend class ::CSimpleEditBox;
    TextChangedEdits();
    bool active = false;
};
CSimpleFontString* CreateFontString(CSimpleFrame*, uint32_t layer, bool show);
CSimpleTexture* CreateTexture(CSimpleFrame*, uint32_t layer, bool show);
CSimpleTitleRegion* CreateTitleRegion(CSimpleFrame*);
void SimpleFrameCreated(CSimpleFrame*);
void SimpleFrameDestroyed(CSimpleFrame*);
void RequireScale(CSimpleFrame*, float);
void RequireLevel(CSimpleFrame*, int32_t);
void RequireRaise(CSimpleFrame*);
void RequireRaiseEntry(CSimpleFrame*);
void RequireStackingMutation(bool tree = false);
// ShowThis may raise its one top-level root during a selected Button click.
// The internal list-edit scope ends before OnShow; it never grants Lua callers
// hierarchy/layout/stacking mutation or relaxes resize/destruction guards.
class VisibilityRaise final {
public:
    ~VisibilityRaise();
    VisibilityRaise(const VisibilityRaise&) = delete;
    VisibilityRaise& operator=(const VisibilityRaise&) = delete;
private:
    friend class ::CSimpleFrame;
    explicit VisibilityRaise(CSimpleFrame*);
    CSimpleFrame* previous;
};
class StackingEdits final {
public:
    ~StackingEdits();
    StackingEdits(const StackingEdits&) = delete;
    StackingEdits& operator=(const StackingEdits&) = delete;
private:
    friend class ::CSimpleTop;
    explicit StackingEdits(CSimpleFrame*);
};
void RaiseFromScript(CSimpleFrame*);
// Only an already-completed raise may be repeated by this frame's OnShow.
// No level/list mutation is admitted while the visibility callback is pinned.
class ShowCallback final {
public:
    ~ShowCallback();
    ShowCallback(const ShowCallback&) = delete;
    ShowCallback& operator=(const ShowCallback&) = delete;
private:
    friend class ::CSimpleFrame;
    explicit ShowCallback(CSimpleFrame*);
    CSimpleFrame* previous;
};
[[noreturn]] void UnavailableFrameFeature();
// Deferral is separate from CSimpleTop::m_destroyed's attribute-permission list.
// Call on the owner outside batch rendering; it is allowed during layer updates.
void DestroyLater(CSimpleFrame*);
void DrainDestroyQueue();
void ClearDirtyBatches(CSimpleTop&);
void PrepareScrollBatches();
void DrawScrollChild(CSimpleFrame*);
// Only the original ScrollFrame callback may enter this traversal. All layout,
// tree, batch mutation and general nested draw guards remain active inside it.
class ScrollRenderScope final {
public:
    ~ScrollRenderScope();
    ScrollRenderScope(const ScrollRenderScope&) = delete;
    ScrollRenderScope& operator=(const ScrollRenderScope&) = delete;
private:
    friend class ::CSimpleScrollFrame;
    friend void DrawScrollChild(CSimpleFrame*);
    ScrollRenderScope(CSimpleFrame*, CSimpleFrame*, void (*)(void*));
    void Draw(CSimpleFrame*, size_t depth);
    ScrollRenderScope* previous;
    CSimpleFrame* parent;
    CSimpleFrame* child;
    void (*callback)(void*);
};
void SetRootProjection(CSimpleTop&);
class CollectScope final {
public:
    CollectScope();
    ~CollectScope();
    CollectScope(const CollectScope&) = delete;
    CollectScope& operator=(const CollectScope&) = delete;
};
class UpdateScope final {
public:
    UpdateScope();
    ~UpdateScope();
    UpdateScope(const UpdateScope&) = delete;
    UpdateScope& operator=(const UpdateScope&) = delete;
};
}}
#endif
