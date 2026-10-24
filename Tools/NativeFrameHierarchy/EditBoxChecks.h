// Original login-sized single-line editing through actual SDL/root queues.
#include "Engine/Source/Frame/CSimpleEditBox.h"
#include "Engine/Source/Os/OsClipboard.h"
static void EditBoxChecks(GlContext& context, LegacyDevice& device, CSimpleTop& root,
                          CSimpleFont& font, bool original, const std::string& snapshot) {
    const int frames = CSimpleFrame::s_frameCount;
    const auto strings = CSimpleFontString::s_allocator.m_dataUsed;
    std::vector<OSEVENT> inputTrace;
    auto pump = [&] {
        context.Window().Pump(); OSEVENT event{};
        while (OsQueueGet(&event.id,&event.param[0],&event.param[1],&event.param[2],&event.param[3])) {
            inputTrace.push_back(event);
            if (Frame::Native::DispatchWindowEvent(context,event)) continue;
            Frame::Native::DispatchInputEvent(context,event);
        }
    };
    pump();
    const auto window = SDL_GetWindowID(context.Window().Handle());
    auto push = [&](SDL_Event event) { event.window.windowID=window; Check(SDL_PushEvent(&event),"EditBox SDL injection"); };
    auto text = [&](const char* value) { SDL_Event e{}; e.type=SDL_EVENT_TEXT_INPUT; e.text.text=value; push(e); };
    auto compose = [&](const char* value,int start,int length) {
        SDL_Event e{};e.type=SDL_EVENT_TEXT_EDITING;e.edit.text=value;e.edit.start=start;e.edit.length=length;push(e);
    };
    auto key = [&](KEY value, bool down=true) {
        Frame::Native::DispatchInputEvent(context,OSEVENT{down?OS_INPUT_KEY_DOWN:OS_INPUT_KEY_UP,{int(value),1,0,123}});
    };
    CStatus status;
    const char* document=R"(<Ui><EditBox name="NativeEditTemplate" virtual="true" autoFocus="false" letters="32">
       <Size x="240" y="48"/><Anchors><Anchor point="BOTTOMLEFT" x="96" y="96"/></Anchors>
       <TextInsets><AbsInset left="4" right="4" top="4" bottom="4"/></TextInsets>
       <Scripts><OnTextChanged>self.changed=(self.changed or 0)+1; self.user=userInput</OnTextChanged></Scripts>
       </EditBox></Ui>)";
    std::unique_ptr<XMLTree,decltype(&XMLTree_Free)> tree(XMLTree_Load(document,std::strlen(document)),&XMLTree_Free);
    Check(bool(tree),"EditBox template parse");
    FrameXML_StoreHashNode(XMLTree_GetRoot(tree.get())->m_child,"NativeEditTemplate",nullptr,&status);tree.reset();
    RegionLua(R"(
        NativeEdit=CreateFrame('EditBox','NativeEdit',nil,'NativeEditTemplate')
        assert(NativeEdit:GetObjectType()=='EditBox' and NativeEdit:IsObjectType('Frame'))
        assert(not NativeEdit:IsAutoFocus() and NativeEdit:GetMaxLetters()==32)
        NativeEdit:SetText('AV'); assert(NativeEdit:GetText()=='AV')
        NativeEdit:SetBlinkSpeed(0)
        NativeEdit.focused=0; NativeEdit.blurred=0
        NativeEdit:SetScript('OnEditFocusGained',function(self)
            self.focused=self.focused+1
            assert(not pcall(self.ClearFocus,self))
            assert(not pcall(CreateFrame,'Frame','InvalidFocusTree'))
        end)
        NativeEdit:SetScript('OnEditFocusLost',function(self) self.blurred=self.blurred+1 end)
        NativeEdit:SetScript('OnTabPressed',function(self) self.tabbed=true end)
        NativeEdit:SetScript('OnEnterPressed',function(self) self.entered=true end)
    )");
    auto* edit=static_cast<CSimpleEditBox*>(CScriptObject::GetScriptObjectByName("NativeEdit",CSimpleEditBox::GetObjectType()));
    Check(edit && uintptr_t(edit)%alignof(CSimpleEditBox)==0,"EditBox pooled alignment");
    edit->m_font->SetFontObject(&font);edit->m_font->UpdateObjects();
    auto ready=Draw(device,root);Check(Ink(ready)>4,"EditBox original FontString ink missing");
    RegionLua("NativeEdit:SetFocus(); assert(NativeEdit:HasFocus() and NativeEdit.focused==1)");
    Check(SDL_TextInputActive(context.Window().Handle()),"Focused EditBox did not request SDL text");
    text("\xc3\xa9\xf0\x9f\x98\x80");pump();
    Check(std::string(edit->m_text)=="AV\xc3\xa9\xf0\x9f\x98\x80","SDL Unicode typing did not reach EditBox");
    key(KEY_LEFT);key(KEY_LEFT,false);Check(edit->m_cursorPos==4,"Scalar cursor step split supplementary UTF-8");
    key(KEY_BACKSPACE);key(KEY_BACKSPACE,false);
    Check(std::string(edit->m_text)=="AV\xf0\x9f\x98\x80","Overlapping UTF-8 backward deletion");
    edit->SetCursorPosition(3);Check(edit->m_cursorPos==2,"Explicit cursor accepted continuation byte");
    edit->HighlightText(2,-1);text("A");pump();Check(std::string(edit->m_text)=="AVA","Selection replacement");
    edit->SetCursorPosition(3);edit->Insert(edit->m_text+1,nullptr,0,0,1);
    Check(std::string(edit->m_text)=="AVAVA","Aliased insertion corrupted source");
    edit->SetText(edit->m_text+2,nullptr);Check(std::string(edit->m_text)=="AVA","Aliased SetText corrupted source");
    edit->SetText("|cffffffff",nullptr);edit->MoveBackward(0);
    Check(edit->m_cursorPos==0,"Quoted-code backward boundary");
    edit->m_textMaxLength=3;edit->SetText("A\xf0\x9f\x98\x80",nullptr);
    Check(std::string(edit->m_text)=="A","Byte limit split a scalar");edit->m_textMaxLength=-1;
    RegionLua("NativeEdit:SetMaxLetters(2); NativeEdit:SetText('AVA'); assert(NativeEdit:GetText()=='AV'); NativeEdit:SetMaxLetters(32)");
    edit->HighlightText(0,-1);edit->CopyToClipboard();
    char* clip=OsClipboardGetString();Check(clip && !std::strcmp(clip,"AV"),"EditBox clipboard copy");OsClipboardFreeString(clip);
    key(KEY_LCONTROL);key(KEY_V);key(KEY_V,false);key(KEY_LCONTROL,false);
    Check(std::string(edit->m_text)=="AV","Modifier-driven paste selection replacement");
    key(KEY_TAB);key(KEY_TAB,false);key(KEY_ENTER);key(KEY_ENTER,false);Draw(device,root);
    RegionLua("assert(NativeEdit.tabbed and NativeEdit.entered and NativeEdit.changed>0)");
    struct EnterFailure final : CSimpleEditBox::EventHandler {
        void unk_0000() override {} void unk_0004() override {}
        void unk_0008() override {} void unk_000C() override {}
        void OnEvent(const CEvent&) override { throw std::runtime_error("Authored Enter callback failure"); }
    } enterFailure;
    const auto enterFlag = root.unused_1250;
    edit->m_enterPressedCallback.m_handler = &enterFailure;
    bool enterThrew = false;
    try { key(KEY_ENTER); } catch (const std::runtime_error&) { enterThrew = true; }
    edit->m_enterPressedCallback.m_handler = nullptr;
    Check(enterThrew && root.unused_1250 == enterFlag,"Enter failure retained root callback state");
    key(KEY_ENTER,false);key(KEY_ENTER);key(KEY_ENTER,false);
    edit->SetText("|rAV",nullptr);Equal(Draw(device,root),Draw(device,root));
    // The owner copies candidates before the next dequeue; widget/row creation
    // is deferred until root update, after the input traversal has ended.
    edit->SetText("A",nullptr);edit->SetCursorPosition(1);
    inputTrace.clear();compose("V",0,1);
    const char* choices[]={"AV","A"};SDL_Event c{};c.type=SDL_EVENT_TEXT_EDITING_CANDIDATES;
    c.edit_candidates.candidates=choices;c.edit_candidates.num_candidates=2;c.edit_candidates.selected_candidate=1;push(c);pump();
    if (!edit->m_imeComposition || std::string(edit->m_text)!="AV" || edit->m_candidatesFrame) {
        std::cerr << "EditBox preedit diagnostics: text=" << edit->m_text << " composition=" << edit->m_imeComposition
                  << " focused=" << (CSimpleEditBox::s_currentFocus==edit) << " candidates-created=" << bool(edit->m_candidatesFrame) << '\n';
        for (const auto& event : inputTrace)
            std::cerr << "EditBox preedit event=" << event.id << " params=" << event.param[0] << ',' << event.param[1] << ',' << event.param[2] << ',' << event.param[3] << '\n';
    }
    Check(edit->m_imeComposition && std::string(edit->m_text)=="AV" && !edit->m_candidatesFrame,
          "EditBox failed preedit or constructed candidates in input callback");
    for(int i=0;i<4;++i) {
        Draw(device,root);
        if(edit->m_candidatesFrame && edit->m_candidatesFrame->m_numVisible>=2) break;
    }
    Check(edit->m_candidatesFrame && edit->m_candidatesFrame->m_isVisible && edit->m_candidatesFrame->m_numVisible>=2,
          "Deferred EditBox candidates did not render rows");
    auto candidates=Draw(device,root);Equal(candidates,Draw(device,root));
    if(!snapshot.empty()) {
        std::ofstream out(snapshot+".editbox-candidates.ppm",std::ios::binary);out<<"P6\n640 480\n255\n";
        for(int y=479;y>=0;--y)for(int x=0;x<640;++x){const auto& p=candidates[y*640+x];out.put(char(p.r));out.put(char(p.g));out.put(char(p.b));}
        Check(bool(out),"EditBox candidate capture");
    }
    inputTrace.clear();text("A");pump();Draw(device,root);
    if (edit->m_imeComposition || std::string(edit->m_text)!="AA" || edit->m_candidatesFrame->m_isVisible) {
        std::cerr << "EditBox commit diagnostics: text=" << edit->m_text << " composition=" << edit->m_imeComposition
                  << " focused=" << (CSimpleEditBox::s_currentFocus==edit) << " candidates=" << edit->m_candidatesFrame->m_isVisible << '\n';
        for (const auto& event : inputTrace)
            std::cerr << "EditBox commit event=" << event.id << " params=" << event.param[0] << ',' << event.param[1] << ',' << event.param[2] << ',' << event.param[3] << '\n';
    }
    Check(!edit->m_imeComposition && std::string(edit->m_text)=="AA" && !edit->m_candidatesFrame->m_isVisible,
          "IME commit duplicated preedit or retained candidates");
    compose("V",0,1);compose("",0,0);pump();Draw(device,root);
    Check(std::string(edit->m_text)=="AA" && !edit->m_imeComposition,"IME cancellation retained preedit");
    RegionLua("NativeEdit:SetPassword(true); NativeEdit:SetText(''); assert(NativeEdit:HasFocus())");
    Check(SDL_TextInputActive(context.Window().Handle()),"Password mode lost SDL text request");
    text("AV");pump();Draw(device,root);
    Check(std::string(edit->m_text)=="AV" && edit->m_textHidden && std::string(edit->m_textHidden)=="**",
          "Password typing/masking");
    edit->HighlightText(0,-1);edit->CopyToClipboard();clip=OsClipboardGetString();
    Check(clip && !*clip,"Password leaked to clipboard");OsClipboardFreeString(clip);
    RegionLua("NativeEdit:SetPassword(false); NativeEdit:SetNumeric(true); NativeEdit:SetText('')");
    text("A12");pump();Check(std::string(edit->m_text)=="12","Numeric input rejected digits or admitted letter");
    RegionLua("NativeEdit:SetNumeric(false); NativeEdit:SetHistoryLines(3); NativeEdit:AddHistoryLine('AV')");
    edit->ForwardHistory();Check(std::string(edit->m_text)=="AV","Original EditBox history");
    Reject([&]{edit->SetHistoryLines(-1);});Reject([&]{edit->SetHistoryLines(129);});
    Reject([&]{edit->Insert(0x110000);});Reject([&]{edit->SetText("\xc0\x80",nullptr);});
    std::string excessive(4096,'A');Reject([&]{edit->SetText(excessive.c_str(),nullptr);});
    {
        Frame::Native::CollectScope collect;
        Reject([&]{edit->SetText("A",nullptr);});
        RegionLua("assert(not pcall(NativeEdit.SetText,NativeEdit,'A'))");
    }
    bool foreign=false;std::thread worker([&]{try{edit->SetText("A",nullptr);}catch(const std::logic_error&){foreign=true;}});worker.join();
    Check(foreign,"Foreign EditBox mutation admitted");
    // Focus transfer keeps exactly one request. Window focus loss releases it;
    // hide, destruction and repeated owners must also return to zero.
    auto* other=static_cast<CSimpleEditBox*>(Create_SimpleEditBox(nullptr));other->SetAutoFocus(0);
    CSimpleEditBox::SetKeyboardFocus(other);
    Check(!edit->m_nativeTextInput && other->m_nativeTextInput && SDL_TextInputActive(context.Window().Handle()),"EditBox focus transfer imbalance");
    other->Hide();Check(!CSimpleEditBox::s_currentFocus && !SDL_TextInputActive(context.Window().Handle()),"Hidden EditBox retained focus/text request");delete other;
    CSimpleEditBox::SetKeyboardFocus(edit);
    Frame::Native::DispatchInputEvent(context,OSEVENT{OS_INPUT_FOCUS,{0,0,0,0}});
    Check(!CSimpleEditBox::s_currentFocus && !SDL_TextInputActive(context.Window().Handle()),"Window focus loss retained EditBox request");
    RegionLua("assert(NativeEdit.blurred>=2); StaleEdit=NativeEdit");
    delete edit;FrameXML_FreeHashNodes();
    RegionLua("assert(not pcall(StaleEdit.GetText,StaleEdit)); StaleEdit=nil");
    Check(CSimpleFrame::s_frameCount==frames && CSimpleFontString::s_allocator.m_dataUsed==strings &&
          !CSimpleEditBox::s_allocator.m_dataUsed && !Ink(Draw(device,root)),"EditBox teardown retained owners/regions/pixels");
    if(original) {
        uint8_t before[16],after[16];
        Check(FrameXML_CheckSignature("Interface\\GlueXML\\GlueXML.toc",nullptr,FrameXML::Native::OriginalInterfaceKey(),before)==3,
              "Original EditBox pre-creation signature");
        tree.reset(FrameXML_LoadXML("Interface\\GlueXML\\AccountLogin.xml",nullptr,&status));
        Check(bool(tree),"Original AccountLogin XML missing");
        XMLNode* selected=nullptr;
        std::function<void(XMLNode*)> find=[&](XMLNode* node) {
            for(;node;node=node->m_next) {
                if(!selected && !SStrCmpI(node->m_name.m_str,"EditBox",STORM_MAX_STR) && node->GetChildByName("Backdrop")) selected=node;
                find(node->m_child);
            }
        };
        find(XMLTree_GetRoot(tree.get()));Check(selected,"Original login field missing");
        // Retain original field/font/backdrop settings in memory. Placement,
        // scripts and typed sample text are authored; this is a signed subset.
        selected->SetAttribute("name","NativeOriginalLoginField");
        selected->SetAttribute("parent","");selected->SetAttribute("inherits","");selected->SetAttribute("autoFocus","false");
        for(auto* child=selected->m_child;child;) {
            auto* next=child->m_next;
            bool keep=false;
            for(const char* tag:{"Size","Backdrop","FontString","TextInsets","HighlightColor"})
                keep=keep || !SStrCmpI(child->m_name.m_str,tag,STORM_MAX_STR);
            if(!keep){child->Detach();delete child;}
            child=next;
        }
        std::unique_ptr<CSimpleFrame> owner(FrameXML_CreateFrame(selected,nullptr,&status));
        auto* field=dynamic_cast<CSimpleEditBox*>(owner.get());Check(field,"Original login field factory");
        field->SetPoint(FRAMEPOINT_CENTER,&root,FRAMEPOINT_CENTER,0,0,1);field->m_cursorBlinkSpeed=0;
        tree.reset();
        CSimpleEditBox::SetKeyboardFocus(field);text("AV");pump();
        std::vector<CImVector> pixels;
        for(int i=0;i<200;++i) {
            pixels=Draw(device,root);
            if(field->m_backdropGenerator && field->m_backdropGenerator->m_backgroundTexture &&
               field->m_backdropGenerator->m_backgroundTexture->m_texture &&
               TextureIsReadyNow(field->m_backdropGenerator->m_backgroundTexture->m_texture)) break;
            SDL_Delay(1);
        }
        Check(std::string(field->m_text)=="AV" && field->m_fontString->m_fontHandle && Ink(pixels)>100,
              "Original login field lost typed text/font/backdrop pixels");
        Equal(pixels,Draw(device,root));
        if(!snapshot.empty()) {
            std::ofstream out(snapshot+".editbox-original.ppm",std::ios::binary);out<<"P6\n640 480\n255\n";
            for(int y=479;y>=0;--y)for(int x=0;x<640;++x){const auto& p=pixels[y*640+x];out.put(char(p.r));out.put(char(p.g));out.put(char(p.b));}
            Check(bool(out),"Original EditBox capture");
        }
        Check(FrameXML_CheckSignature("Interface\\GlueXML\\GlueXML.toc",nullptr,FrameXML::Native::OriginalInterfaceKey(),after)==3 && !std::memcmp(before,after,16),
              "Original field subset post-creation digest changed");
        std::cout<<"original-AccountLogin-editbox-typed-authored-text-font-backdrop-signature-subset=passed\n";
    }
    std::cout<<"frame-editbox-sdl-utf8-selection-clipboard-password-numeric-history-focus-candidates-guards-teardown=passed\n";
}
