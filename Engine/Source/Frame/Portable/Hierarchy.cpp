#include "Hierarchy.h"
#include "Storage.h"
#include "Scripts.h"
#include "Engine/Source/Frame/CSimpleTexture.h"
#include "Engine/Source/Frame/CSimpleRender.h"
#include "Engine/Source/Frame/CSimpleTitleRegion.h"
#include <memory>
#include "Engine/Source/Frame/CSimpleTop.h"
#include "Engine/Source/Frame/CFrameStrataNode.h"
#include "Engine/Source/Services/Camera.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_set>
#include <vector>
namespace Frame { namespace Native {
namespace {
std::unordered_set<CSimpleFrame*> frames;
std::vector<CSimpleFrame*> deferred;
bool updating = false, collecting = false;
ScrollNotify scrollNotify = nullptr;
constexpr size_t MaxDepth = 64;
}
void SetScrollNotify(ScrollNotify function) { RequireTreeMutation(); scrollNotify = function; }
bool HasScrollSupport() { RequireRegions(); return scrollNotify != nullptr; }
void NotifyScroll(CSimpleFrame* frame) {
    RequireRegions();
    if (scrollNotify) scrollNotify(frame);
    else UnavailableFrameFeature();
}
CSimpleFontString* CreateFontString(CSimpleFrame* frame, uint32_t layer, bool show) {
    RequireTreeMutation();
    if (frame) RequireSimpleFrame(frame);
    if (layer >= 5) throw std::invalid_argument("Invalid native FontString draw layer");
    return ConstructRegion<CSimpleFontString>(CSimpleFontString::s_allocator, frame, layer, show);
}
CSimpleTexture* CreateTexture(CSimpleFrame* frame, uint32_t layer, bool show) {
    RequireTreeMutation(); RequireRegionScripts();
    if (frame) RequireSimpleFrame(frame);
    if (layer >= 5) throw std::invalid_argument("Invalid native Texture draw layer");
    return ConstructRegion<CSimpleTexture>(CSimpleTexture::s_allocator, frame, layer, show);
}
CSimpleTitleRegion* CreateTitleRegion(CSimpleFrame* frame) {
    RequireTreeMutation(); RequireSimpleFrame(frame);
    if (frame->m_titleRegion) return static_cast<CSimpleTitleRegion*>(frame->m_titleRegion);
    auto title = std::make_unique<CSimpleTitleRegion>();
    title->m_parent = frame;
    frame->m_titleRegion = title.release();
    return static_cast<CSimpleTitleRegion*>(frame->m_titleRegion);
}
void RequireRoot() {
    RequireRegions(); RequireRenderer();
    if (!CSimpleTop::s_instance) throw std::logic_error("Native Frame root is not installed");
}
void RequireScriptTextMutation() {
    RequireRegions();
    if (collecting) throw std::logic_error("Finish Frame batch collection before changing script text");
}
void RequireTreeMutation() {
    RequireRegions(); RequireDestroyable();
    if (updating || collecting) throw std::logic_error("Defer Frame hierarchy mutation until traversal completes");
}
void RequireStateMutation() {
    RequireRegions(); RequireTreeMutable();
    if (updating || collecting) throw std::logic_error("Defer Frame state edits until traversal completes");
}
void RequireSimpleFrame(CSimpleFrame* frame) {
    RequireRoot();
    if (!frames.count(frame)) throw std::invalid_argument("Frame parent is not live");
}
void RequireParent(CScriptRegion* child, CSimpleFrame* parent) {
    RequireTreeMutation();
    if (parent) RequireSimpleFrame(parent);
    // Validate proposed edges over all descendants before unlinking anything.
    for (auto* leaf : frames) {
        size_t depth = 0;
        double scale = 1;
        for (auto* current = leaf; current;) {
            RequireSimpleFrame(current);
            if (++depth > MaxDepth) throw std::invalid_argument("Cyclic or excessive Frame parent graph");
            scale *= current->m_frameScale;
            current = static_cast<CScriptRegion*>(current) == child ? parent : current->m_parent;
        }
        if (!std::isfinite(float(scale)) || float(scale) <= 0)
            throw std::invalid_argument("Unrepresentable reparented Frame scale");
    }
    for (auto* frame : frames)
        if (static_cast<CScriptRegion*>(frame) == child) RequireLevel(frame, parent ? parent->m_level + 1 : 0);
    if (!child && parent) {
        size_t depth = 1;
        for (auto* current = parent; current; current = current->m_parent)
            if (++depth > MaxDepth) throw std::invalid_argument("Excessive Frame parent graph");
    }
    if (parent && parent->m_level >= static_cast<int32_t>(MaximumFrameLevel()))
        throw std::invalid_argument("Native Frame parent level limit");
}
void RequireLevel(CSimpleFrame* target, int32_t proposed) {
    // Preserve original negative-to-zero clamping and +128 step, while bounding
    // all potentially shifted descendants before the first list mutation.
    const auto limit = static_cast<int32_t>(MaximumFrameLevel());
    if (proposed > limit) throw std::invalid_argument("Native Frame level limit");
    const int delta = std::min(std::max(proposed, 0) - target->m_level, 128);
    for (auto* leaf : frames)
        for (auto* current = leaf; current; current = current->m_parent)
            if (current == target && leaf->m_level + delta > limit)
                throw std::invalid_argument("Native descendant Frame level limit");
}
void RequireRaise(CSimpleFrame* target) {
    RequireStateMutation(); RequireSimpleFrame(target);
    // Predict the original compression, including hidden registered Frames.
    // No list/level changes occur until the final subtree fits the native bound.
    const auto top = CSimpleTop::s_instance->m_strata[target->m_strata]->m_topLevel;
    const auto limit = static_cast<int32_t>(MaximumFrameLevel());
    std::vector<bool> used(size_t(limit) + 1, false);
    std::vector<int> levels(size_t(limit) + 1);
    for (auto* frame : frames)
        if (frame->m_strata == target->m_strata) used[frame->m_level] = true;
    for (size_t i = 0; i < levels.size(); ++i) levels[i] = int(i);
    // Mirror the original scan, including its resumed scan index after a gap.
    // It is not interchangeable with assigning dense ranks to occupied levels.
    int compressedTop = int(top), empty = -1, level = 0;
    while (level < compressedTop) {
        if (!used[level]) {
            if (empty == -1) empty = level;
            ++level;
            continue;
        }
        if (empty == -1) { ++level; continue; }
        const int gap = level - empty;
        for (int moved = level; moved < compressedTop; ++moved) {
            for (auto& planned : levels) if (planned == moved) planned -= gap;
            used[moved - gap] = used[moved];
            used[moved] = false;
        }
        compressedTop -= gap;
        empty = -1;
    }
    if (empty != -1) compressedTop = empty;
    if (compressedTop > limit) throw std::invalid_argument("Native raised Frame level limit");
    const int delta = compressedTop - levels[target->m_level];
    for (auto* leaf : frames)
        for (auto* current = leaf; current; current = current->m_parent)
            if (current == target) {
                const int level = leaf->m_strata == target->m_strata ? levels[leaf->m_level] : leaf->m_level;
                if (level + delta > limit)
                    throw std::invalid_argument("Native raised descendant level limit");
            }
}
void RequireScale(CSimpleFrame* target, float proposed) {
    // Original recursive scale multiplication must remain finite before mutation.
    for (auto* leaf : frames) {
        double scale = 1;
        for (auto* current = leaf; current; current = current->m_parent)
            scale *= current == target ? proposed : current->m_frameScale;
        if (!std::isfinite(float(scale)) || float(scale) <= 0)
            throw std::invalid_argument("Unrepresentable effective Frame scale");
    }
}
void SimpleFrameCreated(CSimpleFrame* frame) { RequireRoot(); frames.insert(frame); }
void SimpleFrameDestroyed(CSimpleFrame* frame) {
    RequireRoot();
    deferred.erase(std::remove(deferred.begin(), deferred.end(), frame), deferred.end());
    frames.erase(frame);
    if (frames.empty()) {
        std::unordered_set<CSimpleFrame*>().swap(frames);
        std::vector<CSimpleFrame*>().swap(deferred);
    }
}
[[noreturn]] void UnavailableFrameFeature() {
    throw std::logic_error("Native Frame XML/Lua factories, input, animation, scrolling and backdrops are not installed");
}
void DestroyLater(CSimpleFrame* frame) {
    RequireSimpleFrame(frame);
    if (std::find(deferred.begin(), deferred.end(), frame) == deferred.end()) deferred.push_back(frame);
}
void DrainDestroyQueue() {
    RequireTreeMutation();
    while (!deferred.empty()) {
        auto* frame = deferred.front();
        deferred.erase(deferred.begin());
        delete frame; // Parent destruction removes queued descendants too.
    }
}
CollectScope::CollectScope() {
    RequireTreeMutation(); collecting = true;
}
CollectScope::~CollectScope() { collecting = false; }
UpdateScope::UpdateScope() {
    RequireTreeMutation(); updating = true;
}
UpdateScope::~UpdateScope() { updating = false; }
void ClearDirtyBatches(CSimpleTop& root) {
    // A Gxu string has one intrusive batch link. Retire every dirty source before
    // moving it to an earlier level/strata/layer, irrespective of rebuild order.
    // Compression can lower m_topLevel while retired source batches above it
    // still own the same intrusive links. Visit every allocated level.
    for (auto* strata : root.m_strata)
        for (uint32_t level = 0; level < strata->m_levels.Count(); ++level) {
            auto* node = strata->m_levels[level];
            for (uint32_t layer = 0; layer < 5; ++layer)
                if (node->m_batchDirty & (1 << layer)) node->m_batches[layer].Clear();
        }
}
void SetRootProjection(CSimpleTop& root) {
    C2Vector offset;
    CameraSetupScreenProjection(root.m_rect, offset, 0.0f, 0);
}
}}
