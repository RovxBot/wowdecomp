// Numeric/transaction contracts for the original Slider. Authored controls,
// no rendering assertions, original-data input, or game Lua/XML substitution.
#include "Engine/Source/Frame/CSimpleSlider.h"
#include <cfloat>
#include <cmath>
#include <limits>
#include <thread>

namespace SliderNumericChecks {
inline void Check(bool good, const char* message) {
    if (!good) throw std::runtime_error(message);
}
struct State {
    float base, range, value, step;
    bool changed, rangeSet, valueSet, buttonDown;
    explicit State(const CSimpleSlider& s)
        : base(s.m_baseValue), range(s.m_range), value(s.m_value), step(s.m_valueStep),
          changed(s.m_changed), rangeSet(s.m_rangeSet), valueSet(s.m_valueSet), buttonDown(s.m_buttonDown) {}
    bool Matches(const CSimpleSlider& s) const {
        return base == s.m_baseValue && range == s.m_range && value == s.m_value && step == s.m_valueStep &&
            changed == bool(s.m_changed) && rangeSet == bool(s.m_rangeSet) && valueSet == bool(s.m_valueSet) &&
            buttonDown == bool(s.m_buttonDown);
    }
};
inline void Run() {
    const auto errors = FrameScript::Native::ScriptErrorCount();
    const auto lua = [&](const char* source) {
        CStatus status;
        Check(FrameScript_ExecuteBuffer(source, std::strlen(source), "authored Slider numeric contracts", &status, nullptr) &&
            status.IsEmpty() && FrameScript::Native::ScriptErrorCount() == errors, "Slider numeric Lua/callback contract failed");
    };
    lua(R"(
        local s=CreateFrame('Slider','NumericSlider')
        NumericOrder=''
        s:SetScript('OnMinMaxChanged',function(self,lo,hi)
            NumericOrder=NumericOrder..'R'
            assert(lo==self:GetMinMaxValues())
        end)
        s:SetScript('OnValueChanged',function(self,value)
            NumericOrder=NumericOrder..'V'
            assert(value==self:GetValue())
        end)
    )");
    auto* slider = static_cast<CSimpleSlider*>(CScriptObject::GetScriptObjectByName("NumericSlider", CSimpleSlider::GetObjectType()));
    Check(slider, "Numeric Slider factory failed");
    const auto rejected = [&](auto action) {
        slider->m_changed = 0;
        const State before(*slider);
        bool failed = false;
        try { action(); } catch (const std::logic_error&) { failed = true; }
        Check(failed && before.Matches(*slider), "Rejected Slider operation changed live state or was admitted");
    };
    // No value has been published yet: range/step validation must still protect
    // future values, and reject before range/step flags or callbacks change.
    slider->SetValueStep(1.0f);
    slider->SetValueStep(FLT_EPSILON);
    rejected([&] { slider->SetMinMaxValues(0.0f, 1000.0f); });
    lua("assert(NumericOrder=='' and NumericSlider:GetValue()==0)");
    slider->SetMinMaxValues(0.0f, 100.0f);
    rejected([&] { slider->SetValueStep(std::numeric_limits<float>::quiet_NaN()); });
    rejected([&] { slider->SetValueStep(std::numeric_limits<float>::infinity()); });
    slider->SetValueStep(1.0f);
    slider->SetMinMaxValues(0.0f, 1000.0f);
    {
        Frame::Native::CallbackScope callback;
        rejected([&] { slider->SetValueStep(2.0f); });
    }
    rejected([&] { slider->SetValueStep(FLT_EPSILON); });
    rejected([&] { slider->SetValueStep(-1.0f); }); // original negative-step clamp would select epsilon
    lua("assert(NumericOrder=='RR' and NumericSlider:GetValueStep()==1)");
    slider->SetValue(250.0f);
    rejected([&] { slider->SetValueStep(FLT_EPSILON); });
    rejected([&] { slider->SetValue(std::numeric_limits<float>::quiet_NaN()); });
    rejected([&] { slider->SetValue(std::numeric_limits<float>::infinity()); });
    rejected([&] { slider->SetMinMaxValues(0.0f, 2147483648.0f); });
    rejected([&] { slider->SetMinMaxValues(-FLT_MAX, FLT_MAX); });
    rejected([&] { slider->SetMinMaxValues(2.0f, 1.0f); });
    lua("assert(NumericOrder=='RRV' and NumericSlider:GetValue()==250)");

    // Nearest representable float below +2^31 is valid; +2^31 itself is not.
    // -2^31 is valid, and its next lower float must reject before conversion.
    const float upper = std::nextafter(2147483648.0f, 0.0f);
    slider->SetMinMaxValues(0.0f, upper);
    slider->SetValue(upper);
    Check(slider->m_value == upper && slider->SnapValueToStep(-2147483648.0f) == -2147483648.0f,
        "Slider rejected a valid signed int32 snap boundary");
    rejected([&] { slider->SnapValueToStep(2147483648.0f); });
    rejected([&] { slider->SnapValueToStep(std::nextafter(-2147483648.0f, -INFINITY)); });
    rejected([&] { slider->SnapValueToStep(std::numeric_limits<float>::infinity()); });
    rejected([&] { slider->SnapValueToStep(std::numeric_limits<float>::quiet_NaN()); });
    Check(slider->SnapValueToStep(2.5f) == 3.0f && slider->SnapValueToStep(-2.5f) == -3.0f,
        "Original positive/negative half-step rounding changed");
    // Finite inputs can overflow offset or half-step addition. A rejected
    // proposal must stay unpublished.
    slider->SetValueStep(FLT_MAX);
    rejected([&] { slider->SetMinMaxValues(0.0f, FLT_MAX); });
    slider->SetMinMaxValues(-FLT_MAX, -FLT_MAX);
    rejected([&] { slider->SnapValueToStep(FLT_MAX); });
    rejected([&] { slider->SnapValueToStep(-FLT_MAX * .25f); });

    // Ordinary floats retain original clamping, epsilon suppression and
    // synchronous range-before-value callbacks, including callback observations.
    slider->SetMinMaxValues(0.0f, 10.0f);
    slider->SetValueStep(1.0f);
    slider->SetValue(4.0f);
    lua(R"(
        local s=NumericSlider
        NumericOrder=''
        s:SetScript('OnValueChanged',function(self,value)
            NumericOrder=NumericOrder..'V'
            assert(value==self:GetValue() and self:GetValueStep()==3)
        end)
        s:SetValueStep(3)
        assert(s:GetValue()==3 and NumericOrder=='V')
        s:SetScript('OnMinMaxChanged',function(self,lo,hi)
            NumericOrder=NumericOrder..'R'
            assert(lo==5 and hi==11 and self:GetValue()==3)
        end)
        s:SetScript('OnValueChanged',function(self,value)
            NumericOrder=NumericOrder..'V'
            assert(value==5 and self:GetValue()==5)
        end)
        s:SetMinMaxValues(5,11)
        assert(NumericOrder=='VRV')
        s:SetValue(-100); s:SetMinMaxValues(5,11); s:SetValueStep(3)
        assert(NumericOrder=='VRV')
        assert(not pcall(s.SetValueStep,s,0/0) and s:GetValueStep()==3)
        assert(not pcall(s.SetMinMaxValues,s,0,3e38))
        assert(NumericOrder=='VRV' and s:GetValue()==5)
        s:SetScript('OnMinMaxChanged',nil); s:SetScript('OnValueChanged',nil)
    )");
    // SetValueStep must preflight state admission even when valueSet is false;
    // no setter may publish a change during callbacks, resize or foreign access.
    {
        Frame::Native::CallbackScope callback;
        rejected([&] { slider->SetValueStep(2.0f); });
        rejected([&] { slider->SetMinMaxValues(0.0f, 5.0f); });
        rejected([&] { slider->SetValue(10.0f); });
    }
    {
        Frame::Native::ResizeScope resize;
        rejected([&] { slider->SetValueStep(2.0f); });
    }
    bool foreign = false;
    const State before(*slider);
    std::thread thread([&] {
        try { slider->SetValueStep(2.0f); } catch (const std::logic_error&) { foreign = true; }
    });
    thread.join();
    Check(foreign && before.Matches(*slider), "Foreign Slider step mutation was admitted");
    slider->SetValueStep(2.0f); // rejection must not strand admission
    lua("NumericOrder=nil");
    delete slider;
    std::cout << "native-slider-snap-int32-bounds-transaction-order-guards=passed\n";
}
}
