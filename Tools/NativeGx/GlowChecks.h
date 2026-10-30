// Distinct-input Glow pixels through actual legacy dispatch. Authored geometry
// and inputs; original-data mode uses the authenticated original FFXGlow bytes.
static void CheckGlow(GlContext &context, LegacyDevice &device) {
    struct PCT2 { float x,y,z; uint8_t color[4]; float u,v,s,t; };
    static_assert(sizeof(PCT2) == 32, "Original Glow PCT2 stride");
    Source full, blur, unused;
    for (unsigned y=0;y<8;++y) for (unsigned x=0;x<8;++x) {
        full.pixels[y*8+x] = x<4 ? 0x29C3 : 0xF37B;
        blur.pixels[y*8+x] = x<4 ? 0xE684 : 0x1ED9;
    }
    CGxTex *textures[3]{};
    CGxTexFlags flags(GxTex_Nearest);
    for (unsigned i=0;i<2;++i)
        GxTexCreate(GxTex_2d,8,8,0,GxTex_Argb4444,GxTex_Argb4444,flags,
                    i ? &blur : &full,Source::Callback,"Glow distinct input",textures[i]);
    flags.m_renderTarget=flags.m_bit8=1;
    GxTexCreate(GxTex_2d,64,32,0,GxTex_Argb8888,GxTex_Argb8888,flags,
                &unused,Source::Callback,"Glow output",textures[2]);
    struct Cleanup { CGxTex **t; ~Cleanup() { for (unsigned i=0;i<3;++i) GxTexDestroy(t[i]); } } cleanup{textures};
    CGxShader *shaders[2]{};
    device.ShaderCreate(shaders,GxSh_Pixel,"Shaders\\Pixel","FFXGlow",2);
    struct ShaderCleanup { LegacyDevice &d; CGxShader **s; ~ShaderCleanup() { d.ShaderDestroy(s[0]); d.ShaderDestroy(s[1]); } } shaderCleanup{device,shaders};
    const auto projection=device.m_projection;
    const auto viewport=device.m_viewport;
    const std::vector<CGxMatrixStack> matrices(device.m_xforms,device.m_xforms+GxXforms_Last);
    device.RsPush();
    struct Restore {
        LegacyDevice &d; C44Matrix projection; CBoundingBox viewport; const std::vector<CGxMatrixStack> &matrices;
        ~Restore() {
            d.DeviceSetRenderTarget(GxBuffers_Color,nullptr,0);
            std::copy(matrices.begin(),matrices.end(),d.m_xforms);
            d.m_projection=projection; d.m_viewport=viewport; d.RsPop();
        }
    } restore{device,projection,viewport,matrices};
    for (auto &matrix:device.m_xforms) matrix.Identity();
    GxXformSetProjection(C44Matrix()); GxXformSetViewport(.125f,.875f,.125f,.875f,0,1);
    GxRsSet(GxRs_VertexShader,static_cast<void*>(nullptr)); GxRsSet(GxRs_PixelShader,shaders[0]);
    GxRsSet(GxRs_Lighting,0); GxRsSet(GxRs_Fog,0); GxRsSet(GxRs_Culling,0);
    GxRsSet(GxRs_DepthTest,0); GxRsSet(GxRs_DepthWrite,0); GxRsSet(GxRs_AlphaRef,0);
    GxRsSet(GxRs_ColorWrite,15); GxRsSet(GxRs_BlendingMode,GxBlend_Opaque);
    for (unsigned i=0;i<4;++i) {
        GxRsSet(EGxRenderState(GxRs_Texture0+i),i<2 ? textures[i] : nullptr);
        GxRsSet(EGxRenderState(GxRs_TexGen0+i),0); GxRsSet(EGxRenderState(GxRs_TextureShader0+i),0);
    }
    PCT2 vertices[8]{};
    auto *buffer=GxBufStream(GxPoolTarget_Vertex,sizeof(PCT2),8);
    auto geometry = [&] { GxPrimVertexPtr(buffer,GxVBF_PCT2); };
    auto upload = [&](int b,int a,bool reverse=false) {
        for (unsigned i=0;i<8;++i) {
            auto &v=vertices[i]; v.x=(i&1)?1.f:-1.f; v.y=(i&2)?1.f:-1.f; v.z=i<4?0.f:.5f;
            v.color[0]=23; v.color[1]=65; v.color[2]=uint8_t(b); v.color[3]=uint8_t(a);
            v.u=reverse?.75f:.25f; v.v=.25f; v.s=reverse?.25f:.75f; v.t=.25f;
        }
        auto *bytes=GxBufLock(buffer); std::memcpy(bytes,vertices,sizeof(vertices)); GxBufUnlock(buffer,0);
        geometry();
    };
    const uint16_t indices[]={0,1,2,3,4,5,6,7,0,1,2,99};
    GxPrimIndexPtr(12,indices);
    CGxBatch quad{GxPrim_TriangleStrip,0,4,0,7};
    const CImVector background(80,48,24,192);
    auto GetInteger=reinterpret_cast<decltype(&glGetIntegerv)>(context.Proc("glGetIntegerv"));
    auto GetEnv=reinterpret_cast<PFNGLGETPROGRAMENVPARAMETERFVARBPROC>(context.Proc("glGetProgramEnvParameterfvARB"));
    auto SetEnv=reinterpret_cast<PFNGLPROGRAMENVPARAMETER4FVARBPROC>(context.Proc("glProgramEnvParameter4fvARB"));
    auto GetProgram=reinterpret_cast<PFNGLGETPROGRAMIVARBPROC>(context.Proc("glGetProgramivARB"));
    auto IsEnabled=reinterpret_cast<decltype(&glIsEnabled)>(context.Proc("glIsEnabled"));
    const float sentinel[3][4]={{.125f,.25f,.5f,1},{.5f,.25f,.75f,1},{.25f,.75f,.125f,1}};
    upload(64,91); GxDraw(&quad,1); // Publish the original pipeline before snapshotting it.
    GLint originalProgram=0,frontFace=0;
    GetProgram(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_BINDING_ARB,&originalProgram);
    GetInteger(GL_FRONT_FACE,&frontFace);
    for (unsigned i=0;i<3;++i) SetEnv(GL_FRAGMENT_PROGRAM_ARB,i,sentinel[i]);
    auto checkState = [&] {
        GLint binding=0,front=0,src=0,dst=0;
        GetProgram(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_BINDING_ARB,&binding);
        GetInteger(GL_FRONT_FACE,&front); GetInteger(GL_BLEND_SRC_RGB,&src); GetInteger(GL_BLEND_DST_RGB,&dst);
        Require(binding==originalProgram && front==frontFace && src==GL_ONE && dst==GL_ZERO &&
                IsEnabled(GL_FRAGMENT_PROGRAM_ARB),"Glow did not restore original program/winding/blending");
        for (unsigned i=0;i<3;++i) {
            float env[4]{}; GetEnv(GL_FRAGMENT_PROGRAM_ARB,i,env);
            Require(!std::memcmp(env,sentinel[i],sizeof(env)),"Glow did not restore fragment environment");
        }
        Require(device.m_appRenderStates[GxRs_Texture0].m_value.m_data.p==textures[0] &&
                device.m_appRenderStates[GxRs_Texture1].m_value.m_data.p==textures[1],"Glow changed original logical inputs");
    };
    auto equation = [&](int b,int a,bool reverse=false,bool modulate=false) {
        const auto f=reverse?0xF37B:0x29C3, t=reverse?0xE684:0x1ED9;
        std::array<double,4> want{{0,0,0,255}};
        const double scene[]={24,48,80,192};
        for (unsigned c=0;c<3;++c) {
            const double fullValue=((f>>(8-4*c))&15)/15., blurValue=((t>>(8-4*c))&15)/15.;
            const double value=fullValue+(b/255.)*(blurValue-fullValue)+(a/255.)*blurValue*blurValue;
            want[c]=std::clamp(value,0.,1.)*(modulate?scene[c]:255.);
        }
        if (modulate) want[3]=scene[3];
        return want;
    };
    auto checkPixel = [&](int x,int y,const std::array<double,4> &want) {
        const auto got=Pixel(device,x,y); const uint8_t channels[]={got.r,got.g,got.b,got.a};
        for (unsigned c=0;c<4;++c)
            if (std::fabs(channels[c]-want[c])>2.)
                throw std::runtime_error("Glow channel "+std::to_string(c)+" got "+std::to_string(channels[c])+" expected "+std::to_string(want[c]));
    };
    for (bool offscreen:{true,false}) {
        device.DeviceSetRenderTarget(GxBuffers_Color,offscreen?textures[2]:nullptr,0);
        CRect size; device.CapsWindowSize(size);
        const int w=offscreen?64:int(size.maxX),h=offscreen?32:int(size.maxY);
        auto pixels = [&](const std::array<double,4> &want) {
            // Opposite sides of the strip's shared diagonal test both windings.
            checkPixel(w*3/8,h*3/8,want); checkPixel(w*5/8,h*5/8,want);
        };
        for (int b:{0,64,192,255}) for (int a:{0,91,255}) for (bool reverse:{false,true}) {
            upload(b,a,reverse); device.SceneClear(GxBufferBit_Color|GxBufferBit_Depth,background);
            GxDraw(&quad,1); pixels(equation(b,a,reverse)); checkState();
        }
        // Saturate BEFORE scene multiplication, with alpha fixed at one even
        // though neither input's alpha is one.
        upload(255,255); GxRsSet(GxRs_BlendingMode,GxBlend_Mod);
        device.SceneClear(GxBufferBit_Color,background); GxDraw(&quad,1); pixels(equation(255,255,false,true));
        GxRsSet(GxRs_BlendingMode,GxBlend_Opaque);
        // Color mask and real scene depth are applied only to the last pass.
        GxRsSet(GxRs_ColorWrite,5); device.SceneClear(GxBufferBit_Color,background); GxDraw(&quad,1);
        auto masked=equation(255,255); masked[1]=48; masked[3]=192; pixels(masked); GxRsSet(GxRs_ColorWrite,15);
        upload(64,0);
        for (unsigned i=4;i<8;++i) vertices[i].color[2]=255;
        auto *bytes=GxBufLock(buffer); std::memcpy(bytes,vertices,sizeof(vertices)); GxBufUnlock(buffer,0);
        CGxBatch layers[]={quad,quad}; layers[1].m_start=4;
        GxRsSet(GxRs_DepthTest,1); GxRsSet(GxRs_DepthWrite,1); GxRsSet(GxRs_DepthFunc,3);
        device.SceneClear(GxBufferBit_Color|GxBufferBit_Depth,background); GxDraw(layers,2); pixels(equation(64,0));
        GxRsSet(GxRs_DepthTest,0); GxRsSet(GxRs_DepthWrite,0);
        // A bad later strip/index rejects before any earlier scene write.
        layers[1].m_start=8; device.SceneClear(GxBufferBit_Color,background);
        Reject([&]{GxDraw(layers,2);},"Glow accepted an invalid later strip fetch");
        pixels({{24,48,80,192}});
        CGxVertexAttrib incomplete[]={{GxVA_Position,4,0,32},{GxVA_Color0,0,12,32},{GxVA_TexCoord0,3,16,32}};
        device.PrimVertexFormat(buffer,incomplete,3);
        device.PrimVertexMask((1u<<GxVA_Position)|(1u<<GxVA_Color0)|(1u<<GxVA_TexCoord0));
        Reject([&]{GxDraw(&quad,1);},"Glow accepted a missing UV1"); pixels({{24,48,80,192}}); geometry();
        C44Matrix shifted; shifted.d0=.5f; GxXformSet(GxXform_Tex1,shifted);
        Reject([&]{GxDraw(&quad,1);},"Glow accepted a transformed UV1"); pixels({{24,48,80,192}});
        GxXformSet(GxXform_Tex1,C44Matrix());
        GxRsSet(GxRs_Texture1,static_cast<void*>(nullptr));
        Reject([&]{GxDraw(&quad,1);},"Glow accepted a missing blurred input"); pixels({{24,48,80,192}});
        GxRsSet(GxRs_Texture1,textures[1]);
        if (offscreen) for (unsigned i=0;i<2;++i) {
            GxRsSet(EGxRenderState(GxRs_Texture0+i),textures[2]);
            Reject([&]{GxDraw(&quad,1);},"Glow accepted attached full/blur input"); pixels({{24,48,80,192}});
            GxRsSet(EGxRenderState(GxRs_Texture0+i),textures[i]);
        }
        upload(64,91); GxRsSet(GxRs_Culling,1);
        GxDraw(&quad,1); pixels(equation(64,91)); checkState(); GxRsSet(GxRs_Culling,0);
        // Final alpha is one, so alpha blending must fully replace the scene.
        GxRsSet(GxRs_BlendingMode,GxBlend_Alpha); device.SceneClear(GxBufferBit_Color,background);
        GxDraw(&quad,1); pixels(equation(64,91)); GxRsSet(GxRs_BlendingMode,GxBlend_Opaque);
    }
    Require(device.m_caps.m_numTmus==1 && device.m_caps.m_shaderConstants[GxSh_Vertex]==94,
            "Glow changed qualified device limits");
    std::cout<<"legacy-gx-glow-distinct-inputs-equation-alpha-clamp-strip-depth-feedback-recovery=passed\n";
}
