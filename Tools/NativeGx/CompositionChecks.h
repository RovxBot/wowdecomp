// Authored transparent material pixels, independent of original login evidence.
static void CheckComposition(GlContext &context, LegacyDevice &device) {
    struct PCT2 { float x,y,z; uint8_t color[4]; float u,v,s,t; };
    static_assert(sizeof(PCT2) == 32, "Original PCT2 stride");
    Source first, second, unused;
    std::fill(first.pixels.begin(), first.pixels.end(), 0x869D);
    for (unsigned y=0; y<8; ++y)
        for (unsigned x=0; x<8; ++x) second.pixels[y*8+x] = x<4 ? 0x4E62 : 0xAB47;
    CGxTex *textures[3]{};
    CGxTexFlags flags(GxTex_Nearest);
    for (unsigned i=0; i<2; ++i)
        Require(GxTexCreate(GxTex_2d,8,8,0,GxTex_Argb4444,GxTex_Argb4444,flags,
                            i ? &second : &first,Source::Callback,"authored composed stage",textures[i]),
                "Composition source creation failed");
    flags.m_renderTarget=1; flags.m_bit8=1;
    Require(GxTexCreate(GxTex_2d,64,32,0,GxTex_Argb8888,GxTex_Argb8888,flags,
                        &unused,Source::Callback,"authored composition scene",textures[2]),
            "Composition scene creation failed");
    struct Cleanup { CGxTex **textures; ~Cleanup() { for (unsigned i=0;i<3;++i) GxTexDestroy(textures[i]); } } cleanup{textures};
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
    GxRsSet(GxRs_VertexShader,static_cast<void*>(nullptr));
    GxRsSet(GxRs_PixelShader,static_cast<void*>(nullptr));
    GxRsSet(GxRs_Texture0,textures[0]); GxRsSet(GxRs_Texture1,textures[1]);
    GxRsSet(GxRs_ColorOp0,GxTexOp_Mod); GxRsSet(GxRs_AlphaOp0,GxTexOp_Mod);
    GxRsSet(GxRs_TexGen0,GxTexGen_Disabled); GxRsSet(GxRs_TexGen1,GxTexGen_Disabled);
    GxRsSet(GxRs_TextureShader0,0); GxRsSet(GxRs_TextureShader1,1);
    GxRsSet(GxRs_AlphaRef,0); GxRsSet(GxRs_ColorWrite,15);
    GxRsSet(GxRs_BlendingMode,GxBlend_Alpha);
    GxRsSet(GxRs_Lighting,0); GxRsSet(GxRs_Fog,0); GxRsSet(GxRs_Culling,0);
    GxRsSet(GxRs_DepthTest,0); GxRsSet(GxRs_DepthWrite,0);
    for (auto &matrix:device.m_xforms) matrix.Identity();
    GxXformSetProjection(C44Matrix()); GxXformSetViewport(.125f,.875f,.125f,.875f,0,1);
    const PCT2 vertices[] = {
        {-1,-1,0,{160,96,240,128},.25f,.25f,.75f,.25f},
        { 3,-1,0,{160,96,240,128},.25f,.25f,.75f,.25f},
        {-1, 3,0,{160,96,240,128},.25f,.25f,.75f,.25f},
        {-1,-1,.5f,{160,96,240,128},.25f,.25f,.75f,.25f},
        { 3,-1,.5f,{160,96,240,128},.25f,.25f,.75f,.25f},
        {-1, 3,.5f,{160,96,240,128},.25f,.25f,.75f,.25f}
    };
    auto *buffer=GxBufStream(GxPoolTarget_Vertex,sizeof(PCT2),6);
    auto *bytes=GxBufLock(buffer); std::memcpy(bytes,vertices,sizeof(vertices)); GxBufUnlock(buffer,0);
    GxPrimVertexPtr(buffer,GxVBF_PCT2);
    const uint16_t indices[]={0,1,2,3,4,5};
    GxPrimIndexPtr(6,indices);
    CGxBatch batch{GxPrim_Triangles,0,3,0,5};
    const CImVector background(80,48,24,192); // Constructor/storage are BGRA.
    auto GetInteger=reinterpret_cast<decltype(&glGetIntegerv)>(context.Proc("glGetIntegerv"));
    auto GetEnv=reinterpret_cast<PFNGLGETPROGRAMENVPARAMETERFVARBPROC>(context.Proc("glGetProgramEnvParameterfvARB"));
    auto SetEnv=reinterpret_cast<PFNGLPROGRAMENVPARAMETER4FVARBPROC>(context.Proc("glProgramEnvParameter4fvARB"));
    auto GetProgram=reinterpret_cast<PFNGLGETPROGRAMIVARBPROC>(context.Proc("glGetProgramivARB"));
    const float sentinel[]={.125f,.25f,.5f,1};
    SetEnv(GL_FRAGMENT_PROGRAM_ARB,0,sentinel);
    GLint originalProgram=0;
    GetProgram(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_BINDING_ARB,&originalProgram);
    auto checkState = [&] {
        float env[4]{}; GLint binding=0,blendSrc=0,blendDst=0;
        GetEnv(GL_FRAGMENT_PROGRAM_ARB,0,env);
        GetProgram(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_BINDING_ARB,&binding);
        GetInteger(GL_BLEND_SRC_RGB,&blendSrc); GetInteger(GL_BLEND_DST_RGB,&blendDst);
        Require(!std::memcmp(env,sentinel,sizeof(env)) && binding==originalProgram &&
                blendSrc==GL_SRC_ALPHA && blendDst==GL_ONE_MINUS_SRC_ALPHA,
                "Composition failed to restore fragment constants/binding or scene blending");
        Require(device.m_appRenderStates[GxRs_Texture0].m_value.m_data.p==textures[0] &&
                device.m_appRenderStates[GxRs_Texture1].m_value.m_data.p==textures[1],
                "Composition changed original texture state");
    };
    auto expected = [&](uint16_t texel, int rgbScale, int alphaScale, int layers, int firstScale=1) {
        std::array<double,4> value{{160./255,96./255,240./255,128./255}};
        const uint16_t a=0x869D,b=texel;
        for (unsigned c=0;c<4;++c) {
            const unsigned shift=c==3 ? 12 : 8-c*4;
            value[c]=std::min(1.,value[c]*((a>>shift)&15)/15.*firstScale);
            value[c]=std::min(1.,value[c]*((b>>shift)&15)/15.*(c==3?alphaScale:rgbScale));
        }
        std::array<double,4> result{{24,48,80,192}};
        for (int layer=0;layer<layers;++layer)
            for (unsigned c=0;c<4;++c) result[c]=value[c]*255*value[3]+result[c]*(1-value[3]);
        return result;
    };
    auto checkPixel = [&](int x,int y,const std::array<double,4> &want) {
        auto got=Pixel(device,x,y);
        const uint8_t channels[]={got.r,got.g,got.b,got.a};
        for (unsigned c=0;c<4;++c)
            if (std::fabs(channels[c]-want[c])>2.0)
                throw std::runtime_error("Composed pixel channel "+std::to_string(c)+" got "+
                    std::to_string(channels[c])+" expected "+std::to_string(want[c]));
    };
    for (bool offscreen:{true,false}) {
        for (unsigned row=0;row<8;++row)
            for (unsigned col=0;col<8;++col) second.pixels[row*8+col]=col<4?0x4E62:0xAB47;
        GxTexUpdate(textures[1],0,0,8,8,1);
        device.DeviceSetRenderTarget(GxBuffers_Color,offscreen?textures[2]:nullptr,0);
        CRect size; device.CapsWindowSize(size);
        const int x=offscreen?32:int(size.maxX/2), y=offscreen?16:int(size.maxY/2);
        for (int rgb:{1,2}) for (int alpha:{1,2}) for (int layers:{1,2}) {
            GxRsSet(GxRs_ColorOp1,rgb==2?GxTexOp_Mod2x:GxTexOp_Mod);
            GxRsSet(GxRs_AlphaOp1,alpha==2?GxTexOp_Mod2x:GxTexOp_Mod);
            batch.m_count=layers*3;
            device.SceneClear(GxBufferBit_Color|GxBufferBit_Depth,background);
            GxDraw(&batch,1);
            checkPixel(x,y,expected(0xAB47,rgb,alpha,layers));
            checkState();
        }
        for (int rgb=0;rgb<GxTexOps_Last;++rgb) for (int alpha=0;alpha<GxTexOps_Last;++alpha) {
            // Scalar oracle for independent RGB/alpha second-stage equations.
            const double previous[]={160./255*6/15,96./255*9/15,240./255*13/15,128./255*8/15};
            const double texel[]={11./15,4./15,7./15,10./15};
            std::array<double,4> value{}, want{{24,48,80,192}};
            for (unsigned channel=0;channel<4;++channel) {
                const auto p=previous[channel],t=texel[channel],a=previous[3];
                const double equations[]={p*t,2*p*t,p+t,p,p*a+t*(1-a),t*a+p*(1-a)};
                value[channel]=std::clamp(equations[channel==3?alpha:rgb],0.,1.);
            }
            for (unsigned channel=0;channel<4;++channel)
                want[channel]=value[channel]*255*value[3]+want[channel]*(1-value[3]);
            GxRsSet(GxRs_ColorOp1,rgb); GxRsSet(GxRs_AlphaOp1,alpha);
            batch.m_count=3; device.SceneClear(GxBufferBit_Color|GxBufferBit_Depth,background);
            GxDraw(&batch,1); checkPixel(x,y,want); checkState();
        }
        GxRsSet(GxRs_ColorOp1,GxTexOp_Mod2x); GxRsSet(GxRs_AlphaOp1,GxTexOp_Mod2x);
        // Separate UV1 and its translation select different halves of texture1.
        device.m_xforms[GxXform_Tex1].m_mtx[0].d0=-.5f;
        device.SceneClear(GxBufferBit_Color|GxBufferBit_Depth,background);
        batch.m_count=3; GxDraw(&batch,1); checkPixel(x,y,expected(0x4E62,2,2,1));
        device.m_xforms[GxXform_Tex1].Identity();
        // Clamp stage0 before Mod2x; clamp stage1 before scene blending too.
        GxRsSet(GxRs_ColorOp0,GxTexOp_Mod2x); GxRsSet(GxRs_AlphaOp0,GxTexOp_Mod2x);
        device.SceneClear(GxBufferBit_Color|GxBufferBit_Depth,background);
        GxDraw(&batch,1); checkPixel(x,y,expected(0xAB47,2,2,1,2));
        GxRsSet(GxRs_ColorOp0,GxTexOp_Mod); GxRsSet(GxRs_AlphaOp0,GxTexOp_Mod);
        // Alpha test and color masks belong to the final scene operation.
        GxRsSet(GxRs_AlphaRef,96);
        device.SceneClear(GxBufferBit_Color|GxBufferBit_Depth,background);
        GxDraw(&batch,1); checkPixel(x,y,{{24,48,80,192}});
        GxRsSet(GxRs_AlphaRef,64);
        GxDraw(&batch,1); checkPixel(x,y,expected(0xAB47,2,2,1));
        GxRsSet(GxRs_AlphaRef,0); GxRsSet(GxRs_ColorWrite,5);
        device.SceneClear(GxBufferBit_Color|GxBufferBit_Depth,background);
        GxDraw(&batch,1);
        auto masked=expected(0xAB47,2,2,1); masked[1]=48; masked[3]=192;
        checkPixel(x,y,masked); checkState(); GxRsSet(GxRs_ColorWrite,15);
        // Near primitive updates real depth only on composition. Far overlap fails.
        GxRsSet(GxRs_DepthTest,1); GxRsSet(GxRs_DepthWrite,1); GxRsSet(GxRs_DepthFunc,0);
        batch.m_count=6;
        device.SceneClear(GxBufferBit_Color|GxBufferBit_Depth,background);
        GxDraw(&batch,1); checkPixel(x,y,expected(0xAB47,2,2,1));
        GxRsSet(GxRs_DepthTest,0); GxRsSet(GxRs_DepthWrite,0);
        // A bad later primitive must reject before publishing an earlier one.
        const uint16_t bad[]={0,1,2,3,4,99}; GxPrimIndexPtr(6,bad);
        device.SceneClear(GxBufferBit_Color|GxBufferBit_Depth,background);
        Reject([&]{GxDraw(&batch,1);},"Composition accepted an invalid later index");
        checkPixel(x,y,{{24,48,80,192}});
        GxPrimIndexPtr(6,indices);
        GxRsSet(GxRs_ColorOp1,GxTexOps_Last);
        Reject([&]{GxDraw(&batch,1);},"Composition silently substituted an unsupported combiner");
        checkPixel(x,y,{{24,48,80,192}});
        GxRsSet(GxRs_ColorOp1,GxTexOp_Mod2x);
        GxDraw(&batch,1); checkPixel(x,y,expected(0xAB47,2,2,2));
        // White stage1/Mod is an identity. Compare the real single-stage
        // fixed GPU pipeline with composed lighting, secondary specular and fog.
        std::fill(second.pixels.begin(),second.pixels.end(),0xFFFF);
        GxTexUpdate(textures[1],0,0,8,8,1);
        const std::vector<CGxApiLight> lights(device.m_lights,device.m_lights+4);
        for (auto &light:device.m_lights) light.m_enable=0;
        auto &light=device.m_lights[0];
        light.m_enable=1; light.m_dir=C4Vector(0,0,-1,0);
        light.m_ambColor=C3Vector(.15f,.2f,.1f); light.m_dirColor=C3Vector(.5f,.4f,.3f);
        light.m_specColor=C3Vector(.7f,.6f,.5f); light.m_constantAttenuation=1;
        GxRsSet(GxRs_MatSpecular,int32_t(0xFFA07850));
        GxRsSet(GxRs_MatSpecularExp,8.f);
        GxRsSet(GxRs_MatEmissive,int32_t(0x00100C08));
        GxRsSet(GxRs_FogStart,0.f); GxRsSet(GxRs_FogEnd,1.f);
        GxRsSet(GxRs_FogColor,int32_t(0xFF1E6EB4));
        C44Matrix view; view.d2=-.5f; GxXformSetView(view);
        GxRsSet(GxRs_ColorOp1,GxTexOp_Mod); GxRsSet(GxRs_AlphaOp1,GxTexOp_Mod);
        batch.m_count=3;
        for (int lighting:{0,1}) for (int fog:{0,1}) for (int op=0;op<GxTexOps_Last;++op) {
            GxRsSet(GxRs_Lighting,lighting); GxRsSet(GxRs_Fog,fog);
            GxRsSet(GxRs_ColorOp0,op); GxRsSet(GxRs_AlphaOp0,GxTexOps_Last-1-op);
            GxRsSet(GxRs_Texture1,static_cast<void*>(nullptr));
            device.SceneClear(GxBufferBit_Color|GxBufferBit_Depth,background); GxDraw(&batch,1);
            const auto reference=Pixel(device,x,y);
            GxRsSet(GxRs_Texture1,textures[1]);
            device.SceneClear(GxBufferBit_Color|GxBufferBit_Depth,background); GxDraw(&batch,1);
            checkPixel(x,y,{{double(reference.r),double(reference.g),double(reference.b),double(reference.a)}});
            checkState();
        }
        std::copy(lights.begin(),lights.end(),device.m_lights);
        GxRsSet(GxRs_Fog,0); GxRsSet(GxRs_Lighting,0);
        GxRsSet(GxRs_ColorOp0,GxTexOp_Mod); GxRsSet(GxRs_AlphaOp0,GxTexOp_Mod);
        GxXformSetView(C44Matrix());
    }
    Require(device.m_caps.m_numTmus==1 && device.m_caps.m_shaderConstants[GxSh_Vertex]==94,
            "Composition changed qualified device limits");
    std::cout<<"legacy-gx-transparent-composition-rgb-alpha-uv-transform-overlap-depth-clamp-recovery=passed\n";
}
