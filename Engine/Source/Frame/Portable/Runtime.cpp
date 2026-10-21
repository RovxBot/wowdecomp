// Native lifetime/validation around the original 12340 layout consumers.
#include "Runtime.h"
#include "Engine/Source/Frame/CLayoutFrame.h"
#include "Engine/Source/Frame/CFramePoint.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <unordered_set>
#include <unordered_map>
namespace Frame { namespace Native {
namespace {
bool installed = false;
std::unordered_set<CLayoutFrame*> frames;
size_t resizing = 0;
std::unordered_set<CFramePoint*> points;
// Initial bounded layout surface; larger graphs require separate qualification.
constexpr size_t MaxFrames = 1024, MaxDepth = 64;
size_t Depth(CLayoutFrame* node, CLayoutFrame* target, CLayoutFrame* relative,
             std::unordered_set<CLayoutFrame*>& active,
             std::unordered_map<CLayoutFrame*, size_t>& memo) {
    RequireFrame(node);
    if (active.size() >= MaxDepth || active.count(node))
        throw std::invalid_argument("Cyclic or excessive layout anchor dependency");
    auto found = memo.find(node);
    if (found != memo.end()) return found->second;
    active.insert(node);
    size_t depth = 1;
    if (node == target) depth = 1 + Depth(relative, target, relative, active, memo);
    for (auto* point : node->m_framePoints)
        if (point && !(point->m_flags & 8) && point->m_relative)
            depth = std::max(depth, 1 + Depth(point->m_relative, target, relative, active, memo));
    active.erase(node);
    if (depth > MaxDepth) throw std::invalid_argument("Excessive layout anchor dependency");
    return memo[node] = depth;
}
}
Runtime::Runtime() {
    FrameXML::Native::RequireOwner();
    if (installed) throw std::logic_error("Only one layout runtime may be installed");
    installed = true;
}
Runtime::~Runtime() {
    try {
        RequireNoObjects();
        CFramePoint::s_framePointHeap.Clear(nullptr, 0, 1);
        std::unordered_set<CLayoutFrame*>().swap(frames);
        std::unordered_set<CFramePoint*>().swap(points);
        installed = false;
    } catch (...) { std::terminate(); }
}
void RequireOwner() {
    FrameXML::Native::RequireOwner();
    if (!installed) throw std::logic_error("Native layout runtime is not installed");
}
void RequireNoObjects() {
    RequireOwner();
    if (!frames.empty() || CFramePoint::s_framePointHeap.m_dataUsed || resizing || !points.empty())
        throw std::logic_error("Close layout frames/points and finish resize before shutdown");
}
void FrameCreated(CLayoutFrame* frame) {
    RequireOwner();
    if (frames.size() >= MaxFrames) throw std::length_error("Native layout frame limit");
    frames.insert(frame);
}
void PointCreated(CFramePoint* point) { RequireOwner(); points.insert(point); }
void PointDestroyed(CFramePoint* point) { RequireOwner(); points.erase(point); }
void FrameDestroyed(CLayoutFrame* frame) {
    RequireOwner();
    // Owned dependent anchors were retired by DestroyLayout. Standalone points
    // have no resize-list registration, but must not retain a dangling origin.
    for (auto* point : points)
        if (point->m_relative == frame) point->MarkUnused();
    frames.erase(frame);
}
void RequireFrame(CLayoutFrame* frame) {
    RequireOwner();
    if (!frame || !frames.count(frame)) throw std::invalid_argument("Layout anchor is not a live frame");
}
void RequirePoint(uint32_t point) {
    RequireOwner();
    if (point >= FRAMEPOINT_NUMPOINTS) throw std::invalid_argument("Invalid layout anchor index");
}
void RequireAnchor(CLayoutFrame* frame, CLayoutFrame* relative) {
    RequireFrame(frame);
    RequireFrame(relative);
    std::unordered_set<CLayoutFrame*> active;
    std::unordered_map<CLayoutFrame*, size_t> memo;
    for (auto* node : frames) Depth(node, frame, relative, active, memo);
}
void RequireFinite(float value) {
    RequireOwner();
    if (!std::isfinite(value)) throw std::invalid_argument("Nonfinite layout coordinate");
}
void RequireDimension(float value) {
    RequireFinite(value);
    if (value < 0.0f) throw std::invalid_argument("Negative layout dimension");
}
void RequireDestroyable() {
    RequireOwner();
    if (resizing) throw std::logic_error("Finish resize callbacks before destroying layout objects");
}
ResizeScope::ResizeScope() { RequireOwner(); ++resizing; }
ResizeScope::~ResizeScope() { --resizing; }
}}
