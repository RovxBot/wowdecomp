#include "Engine/Source/FrameXML/Portable/Factories.h"
static void XMLChecks(LegacyDevice& device, CSimpleTop& root, CSimpleFont& font) {
    CStatus status;
    auto* parent = Create_SimpleFrame(nullptr);
    parent->SetName("NativeXMLParent");
    parent->SetSize(.8f, .6f);
    parent->SetPoint(FRAMEPOINT_BOTTOMLEFT, &root, FRAMEPOINT_BOTTOMLEFT, .05f, .05f, 1);
    const char* document = R"(<Ui>
      <Frame name="NativeXMLBase" virtual="true" parent="NativeXMLParent" frameStrata="HIGH">
        <Size x="300" y="120"/><Anchors><Anchor point="BOTTOMLEFT" x="8" y="8"/></Anchors>
        <TitleRegion setAllPoints="true"/>
        <Layers><Layer level="OVERLAY"><FontString name="$parentText" parentKey="Label"
            setAllPoints="true" text="NativeXMLToken"/></Layer></Layers>
        <Frames><Frame name="$parentChild" setAllPoints="true"/></Frames>
      </Frame>
      <Frame name="NativeXMLDerived" virtual="true" inherits="NativeXMLBase"/>
      <Frame name="NativeXMLCycleA" virtual="true" inherits="NativeXMLCycleB"/>
      <Frame name="NativeXMLCycleB" virtual="true" inherits="NativeXMLCycleA"/>
      <Frame name="NativeXMLUnsupported" virtual="true"><Layers><Layer><Texture/></Layer></Layers></Frame>
      <FontString name="NativeXMLTextTemplate" virtual="true"/>
    </Ui>)";
    std::unique_ptr<XMLTree, decltype(&XMLTree_Free)> tree(XMLTree_Load(document, std::strlen(document)), &XMLTree_Free);
    Check(bool(tree), "Authored Frame factory XML parse");
    auto* first = XMLTree_GetRoot(tree.get())->m_child;
    auto* label = first->GetChildByName("Layers")->m_child->m_child;
    label->SetAttribute("inherits", font.GetName());
    for (auto* node = first; node;) {
        auto* next = node->m_next;
        FrameXML_StoreHashNode(node, node->GetAttributeByName("name"), nullptr, &status);
        node = next;
    }
    tree.reset(); // Templates must outlive their original XMLTree.
    const int nesting = lua_taintexpected;
    RegionLua(R"(
        NativeXMLToken = '2 |4template:templates;'
        for _,bad in ipairs({'NativeXMLCycleA','MissingTemplate','NativeXMLUnsupported','NativeXMLTextTemplate'}) do
            assert(not pcall(CreateFrame,'Frame','RejectedXML',nil,bad))
            assert(RejectedXML == nil)
        end
        assert(not pcall(CreateFrame,'Button','RejectedXML'))
        assert(not pcall(CreateFrame,'Frame','RejectedXML',{[0]=NativeXMLParent[0]}))
        assert(not pcall(CreateFrame,'Frame','RejectedXML',nil,nil,'2147483648'))
        assert(not pcall(CreateFrame,'Frame','RejectedXML',nil,nil,'nan'))
        local f = CreateFrame('Frame','$parentInstance',nil,'NativeXMLDerived',17)
        assert(f == NativeXMLParentInstance and f:GetParent() == NativeXMLParent)
        assert(f.Label == NativeXMLParentInstanceText and f.Label:GetText() == '2 templates')
        assert(f.Label:GetParent() == f and f:GetChildren() == NativeXMLParentInstanceChild)
        assert(f:GetTitleRegion():GetParent() == f)
        local w,h = f:GetSize()
        assert(math.abs(w-300) < .001 and math.abs(h-120) < .001)
        local override = CreateFrame('Frame','NativeXMLOverride',f,'NativeXMLBase')
        assert(override:GetParent() == f and NativeXMLOverrideText:GetParent() == override)
        StaleXMLFrame,StaleXMLText = f,f.Label
    )");
    auto* instance = static_cast<CSimpleFrame*>(CScriptObject::GetScriptObjectByName("NativeXMLParentInstance", CSimpleFrame::GetObjectType()));
    Check(instance && instance->m_id == 17 && instance->m_strata == FRAME_STRATA_HIGH &&
          !instance->m_isLoading && !(instance->m_titleRegion->m_layoutFlags & 0x200), "Original Frame XML id/strata/load completion");
    Check(status.IsEmpty() && lua_taintexpected == nesting, "XML factory left status or taint nesting");
    auto pixels = Draw(device, root);
    Check(Ink(pixels) > 4, "XML-template FontStrings did not render");
    Equal(pixels, Draw(device, root));
    // A failure after template acquisition must release both taint and its lock.
    bool threw = false;
    XMLNode inherited(nullptr, "Frame"); inherited.SetAttribute("inherits", "NativeXMLDerived");
    try { FrameXML::Native::ApplyTemplates(&inherited, [](XMLNode*) { throw std::invalid_argument("Authored template consumer failure"); }); }
    catch (const std::invalid_argument&) { threw = true; }
    Check(threw && lua_taintexpected == nesting, "Template exception did not unwind taint");
    const char* taint = nullptr; bool locked = true;
    Check(FrameXML_AcquireHashNode("NativeXMLDerived", taint, locked) && !locked, "Template exception retained lock");
    Reject([&] { FrameXML_FreeHashNodes(); });
    FrameXML_ReleaseHashNode("NativeXMLDerived");
    // Failure during a later nested child rolls back the entire allocated tree.
    const char* invalid = R"(<Frame name="RolledBackXML"><Frames><Frame name="$parentChild" parent="AbsentXMLParent"/></Frames></Frame>)";
    tree.reset(XMLTree_Load(invalid, std::strlen(invalid)));
    Reject([&] { FrameXML_CreateFrame(XMLTree_GetRoot(tree.get()), nullptr, &status); });
    tree.reset();
    RegionLua("assert(RolledBackXML == nil and RolledBackXMLChild == nil)");
    {
        Frame::Native::CollectScope traversal;
        RegionLua("assert(not pcall(CreateFrame,'Frame','RejectedTraversal')); assert(RejectedTraversal == nil)");
    }
    delete parent;
    RegionLua(R"(
        assert(NativeXMLParent == nil and NativeXMLParentInstance == nil and NativeXMLOverride == nil)
        assert(not pcall(StaleXMLFrame.GetName,StaleXMLFrame) and not pcall(StaleXMLText.GetText,StaleXMLText))
        StaleXMLFrame,StaleXMLText,NativeXMLToken = nil,nil,nil
        collectgarbage('collect')
    )");
    Check(!CSimpleFrame::s_allocator.m_dataUsed && !CSimpleFontString::s_allocator.m_dataUsed,
          "XML factory teardown retained pool slots");
    Check(!Ink(Draw(device, root)), "Destroyed XML hierarchy retained text batches");
    FrameXML_FreeHashNodes();
    std::cout << "frame-xml-registry-createframe-named-parent-retained-template-nested-layer-render-rollback=passed\n";
}
