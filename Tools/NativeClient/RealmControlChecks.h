// Authored controls for the observed RealmList OnShow/row update. No original
// Lua/XML, server rows or game assets are embedded in this fixture.
namespace RealmControlChecks {
static CSimpleFrame* ownedRoot=nullptr;
static bool Owns(CScriptRegion* region) {
    CScriptObject* object=region;
    for (unsigned i=0;object && i<64;++i,object=object->GetScriptObjectParent())
        if (object==ownedRoot) return true;
    return false;
}
static void Run(CSimpleTop& root,OsNative::Window& window) {
    auto errors=FrameScript::Native::ScriptErrorCount();
    const auto lua=[&](const char* source) {
        FrameScript_Execute(source,"authored realm control publication",nullptr);
        if (FrameScript::Native::ScriptErrorCount()!=errors)
            throw std::runtime_error("Realm controls: "+FrameScript::Native::LastScriptError());
    };
    lua(R"(
        local p=CreateFrame('Frame','AuthoredRealmControls'); p:SetSize(100,100); p:SetPoint('CENTER'); p:Hide();
        local foreign=CreateFrame('Frame','ForeignRealmControls'); foreign:SetSize(10,10); foreign:SetPoint('CENTER');
        for i=1,8 do
            local t=CreateFrame('Button','AuthoredRealmTab'..i,p); t:SetSize(10,10); t:SetPoint('CENTER');
        end
        local h=p:CreateTexture('AuthoredRealmHighlight'); h:SetSize(5,5); h:SetPoint('CENTER');
        local s=CreateFrame('Slider','AuthoredRealmSlider',p); s:SetSize(10,40); s:SetPoint('CENTER'); s:SetMinMaxValues(0,10);
        local opener=CreateFrame('Button','AuthoredRealmOpener',p); opener:SetSize(20,20); opener:SetPoint('BOTTOMLEFT');
        local style=CreateFont('AuthoredRealmStyle');
        s:SetScript('OnValueChanged',function()
            assert(not pcall(foreign.SetWidth,foreign,40));
            assert(not pcall(p.SetFrameLevel,p,3));
            assert(not pcall(CreateFrame,'Frame','ForbiddenRealmChild',p));
            h:SetWidth(25);
        end);
        local function update()
            for i=1,8 do _G['AuthoredRealmTab'..i]:Hide() end;
            local tab=AuthoredRealmTab1; tab:Disable(); tab:LockHighlight(); tab:UnlockHighlight();
            tab:SetNormalFontObject(style); tab:SetHighlightFontObject(style); tab:SetDisabledFontObject(style);
            h:ClearAllPoints(); h:SetPoint('CENTER',tab); h:SetWidth(587);
            assert(not pcall(h.SetWidth,h,'bad-width'));
            assert(not pcall(foreign.SetWidth,foreign,44));
            assert(not pcall(tab.SetParent,tab,foreign));
            assert(not pcall(tab.Raise,tab));
            s:SetValue(s:GetValue()==0 and 1 or 0);
            realmControlsUpdated=(realmControlsUpdated or 0)+1;
            RealmControlGuards();
        end
        p:SetScript('OnShow',update); opener:SetScript('OnClick',update);
    )");
    ownedRoot=static_cast<CSimpleFrame*>(CScriptObject::GetScriptObjectByName("AuthoredRealmControls",CSimpleFrame::GetObjectType()));
    ownedRoot->SetFrameStrata(FRAME_STRATA_TOOLTIP); ownedRoot->SetFrameFlag(1,1);
    auto* foreign=static_cast<CSimpleFrame*>(CScriptObject::GetScriptObjectByName("ForeignRealmControls",CSimpleFrame::GetObjectType()));
    struct Context { CSimpleTop* root; OsNative::Window* window; CSimpleFrame* foreign; } context{&root,&window,foreign};
    lua_pushlightuserdata(s_context,&context);
    lua_pushcclosure(s_context,+[](lua_State* L)->int {
        auto& c=*static_cast<Context*>(lua_touserdata(L,lua_upvalueindex(1)));
        const auto reject=[](auto action) {
            bool failed=false; try { action(); } catch(const std::logic_error&) { failed=true; }
            if (!failed) throw std::runtime_error("Realm controls widened an unrelated lifetime/input guard");
        };
        reject([&] { Frame::Native::RequireTreeMutation(); });
        reject([&] { Frame::Native::RequireDestroyable(); });
        reject([&] { c.foreign->SetWidth(99); });
        reject([&] { Frame::Native::MouseButtonInput nested(nullptr); });
        reject([&] { Frame::Native::ControlPublication nested; });
        reject([&] { c.root->OnLayerUpdate(.016f); });
        reject([&] { c.root->OnLayerRender(); });
        reject([&] { Frame::Native::DispatchInputEvent(*c.window,OSEVENT{OS_INPUT_MOUSE_UP,{MOUSE_BUTTON_LEFT,0,0,1}}); });
        return 0;
    },1);
    lua_setglobal(s_context,"RealmControlGuards");
    Frame::Native::SetControlRootQuery(Owns);
    struct Reset { ~Reset() { Frame::Native::SetControlRootQuery(nullptr); ownedRoot=nullptr; } } reset;
    { Frame::Native::ControlPublication publish; lua("AuthoredRealmControls:Show(); assert(realmControlsUpdated==1)"); }
    // A protected Lua error must not retain admission after publication.
    { Frame::Native::CallbackScope callback;
      lua("assert(not pcall(AuthoredRealmHighlight.SetWidth,AuthoredRealmHighlight,41))"); }
    // The actual original selected Button dispatch uses the same qualified roots.
    auto* opener=static_cast<CSimpleButton*>(CScriptObject::GetScriptObjectByName("AuthoredRealmOpener",CSimpleButton::GetObjectType()));
    root.OnLayerUpdate(.016f);
    CRect rect; if (!opener->GetRect(&rect)) throw std::runtime_error("Realm fixture button rectangle unavailable");
    float x,y; DDCToNDC((rect.minX+rect.maxX)*.5f,(rect.minY+rect.maxY)*.5f,&x,&y);
    int width,height,pixelWidth,pixelHeight; window.Size(width,height,pixelWidth,pixelHeight);
    const auto deliver=[&](OSINPUT id,int button=0) {
        Frame::Native::DispatchInputEvent(window,OSEVENT{id,{button,int(x*width),int((1-y)*height),2}});
    };
    deliver(OS_INPUT_MOUSE_MOVE);
    if (root.m_mouseFocus!=opener) throw std::runtime_error("Realm fixture click target was not selected");
    deliver(OS_INPUT_MOUSE_DOWN,MOUSE_BUTTON_LEFT); deliver(OS_INPUT_MOUSE_UP,MOUSE_BUTTON_LEFT);
    lua("assert(realmControlsUpdated==2)");
    lua("AuthoredRealmControls:SetScript('OnUpdate',function() AuthoredRealmTab8:Hide(); assert(not pcall(AuthoredRealmHighlight.SetWidth,AuthoredRealmHighlight,77)) end)");
    root.OnLayerUpdate(.016f);
    lua("AuthoredRealmControls:SetScript('OnUpdate',nil); AuthoredRealmControls:Hide()");
    lua("AuthoredRealmControls:SetScript('OnShow',function() error('authored realm callback failure') end)");
    ++errors;
    { Frame::Native::ControlPublication publish; lua("AuthoredRealmControls:Show()"); }
    if (FrameScript::Native::LastScriptError().find("authored realm callback failure")==std::string::npos)
        throw std::runtime_error("Realm callback failure lost attribution");
    { Frame::Native::CallbackScope callback;
      lua("assert(not pcall(AuthoredRealmHighlight.SetWidth,AuthoredRealmHighlight,41))"); }
    lua("AuthoredRealmControls:Hide(); AuthoredRealmControls:SetScript('OnShow',nil); RealmControlGuards=nil");
    std::cout<<"native-realm-controls-publication-click-lua-error-root-and-lifetime-guards=passed\n";
}
}
