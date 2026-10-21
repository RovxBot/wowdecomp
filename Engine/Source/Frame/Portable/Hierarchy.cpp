#include "Hierarchy.h"
#include "Engine/Source/Frame/CSimpleTop.h"
#include "Engine/Source/Frame/CFrameStrataNode.h"
#include "Engine/Source/Services/Camera.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <vector>
namespace Frame { namespace Native {
namespace {
std::unordered_set<CSimpleFrame*> frames;
std::vector<CSimpleFrame*> deferred;
bool updating = false;
constexpr size_t MaxDepth = 64;
}
void RequireRoot() {
    RequireRegions(); RequireRenderer();
    if (!CSimpleTop::s_instance) throw std::logic_error("Native Frame root is not installed");
}
void RequireTreeMutation() {
    RequireRegions(); RequireDestroyable();
    if (updating) throw std::logic_error("Defer Frame hierarchy mutation until layer update completes");
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
        for (auto* current = leaf; current;) {
            RequireSimpleFrame(current);
            if (++depth > MaxDepth) throw std::invalid_argument("Cyclic or excessive Frame parent graph");
            current = static_cast<CScriptRegion*>(current) == child ? parent : current->m_parent;
        }
    }
    if (!child && parent) {
        size_t depth = 1;
        for (auto* current = parent; current; current = current->m_parent)
            if (++depth > MaxDepth) throw std::invalid_argument("Excessive Frame parent graph");
    }
    if (parent && parent->m_level >= 128)
        throw std::invalid_argument("Native Frame parent level limit");
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
UpdateScope::UpdateScope() {
    RequireTreeMutation(); updating = true;
}
UpdateScope::~UpdateScope() { updating = false; }
void ClearDirtyBatches(CSimpleTop& root) {
    // A Gxu string has one intrusive batch link. Retire every dirty source before
    // moving it to an earlier level/strata/layer, irrespective of rebuild order.
    for (auto* strata : root.m_strata)
        for (uint32_t level = 0; level < strata->m_topLevel; ++level) {
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
