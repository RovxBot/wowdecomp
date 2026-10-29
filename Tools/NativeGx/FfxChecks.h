// Authored filter fixtures. Original shader containers are read externally in
// original-data mode; no game bytes are embedded in this fixture.
static Bytes FilterPackage(bool gaussian) {
    std::string source = "!!ARBfp1.0\nTEMP sample, sum;\nMOV sum, {0,0,0,0};\n";
    for (unsigned i = 0; i < 4; ++i) {
        source += "TEX sample, fragment.texcoord[" + std::to_string(i) + "], texture[" +
            std::to_string(i) + "], 2D;\nMAD sum, sample, " +
            (gaussian ? (i == 0 || i == 3 ? "0.125" : "0.375") : "0.25") + ", sum;\n";
    }
    source += "MOV result.color, sum;\n";
    if (!gaussian) source += "MOV result.color.w, fragment.color.primary.w;\n";
    source += "END\n";
    return Package(source.c_str(), 2);
}
static void CheckFfxFilters(GlContext &context, LegacyDevice &device) {
    struct PCT4 { float x,y,z; uint8_t color[4]; float uv[4][2]; };
    static_assert(sizeof(PCT4) == 48, "Original FFX PCT4 stride");
    Source source, spare;
    const uint16_t texels[] = {0x1458, 0x5792, 0x9B36, 0xDE71};
    for (unsigned y = 0; y < 8; ++y) for (unsigned x = 0; x < 8; ++x)
        source.pixels[y*8+x] = texels[x/2];
    CGxTex *input = nullptr, *other = nullptr, *output = nullptr;
    auto flags = CGxTexFlags(GxTex_Nearest);
    GxTexCreate(GxTex_2d,8,8,0,GxTex_Argb4444,GxTex_Argb4444,flags,&source,Source::Callback,"filter input",input);
    GxTexCreate(GxTex_2d,8,8,0,GxTex_Argb4444,GxTex_Argb4444,flags,&spare,Source::Callback,"unequal filter input",other);
    flags.m_renderTarget = flags.m_bit8 = 1;
    GxTexCreate(GxTex_2d,64,32,0,GxTex_Argb8888,GxTex_Argb8888,flags,&spare,Source::Callback,"filter output",output);
    struct Cleanup { CGxTex *a,*b,*c; ~Cleanup() { GxTexDestroy(a); GxTexDestroy(b); GxTexDestroy(c); } } cleanup{input,other,output};
    const auto projection = device.m_projection;
    const auto viewport = device.m_viewport;
    const std::vector<CGxMatrixStack> matrices(device.m_xforms, device.m_xforms+GxXforms_Last);
    device.RsPush();
    struct Restore {
        LegacyDevice &d; C44Matrix projection; CBoundingBox viewport; const std::vector<CGxMatrixStack> &matrices;
        ~Restore() {
            d.DeviceSetRenderTarget(GxBuffers_Color,nullptr,0);
            std::copy(matrices.begin(),matrices.end(),d.m_xforms);
            d.m_projection = projection; d.m_viewport = viewport; d.RsPop();
        }
    } restore{device,projection,viewport,matrices};
    for (auto &matrix : device.m_xforms) matrix.Identity();
    GxXformSetProjection(C44Matrix()); GxXformSetViewport(.125f,.875f,.125f,.875f,0,1);
    GxRsSet(GxRs_VertexShader,static_cast<void*>(nullptr));
    GxRsSet(GxRs_Lighting,0); GxRsSet(GxRs_Fog,0); GxRsSet(GxRs_Culling,0);
    GxRsSet(GxRs_DepthTest,0); GxRsSet(GxRs_DepthWrite,0);
    GxRsSet(GxRs_AlphaRef,0); GxRsSet(GxRs_ColorWrite,15); GxRsSet(GxRs_BlendingMode,GxBlend_Opaque);
    for (unsigned i = 0; i < 4; ++i) {
        GxRsSet(EGxRenderState(GxRs_Texture0+i),input);
        GxRsSet(EGxRenderState(GxRs_TexGen0+i),0);
        GxRsSet(EGxRenderState(GxRs_TextureShader0+i),0);
    }
    const PCT4 vertices[] = {
        {-1,-1,0,{255,255,255,91},{{.125f,.25f},{.375f,.25f},{.625f,.25f},{.875f,.25f}}},
        { 3,-1,0,{255,255,255,91},{{.125f,.25f},{.375f,.25f},{.625f,.25f},{.875f,.25f}}},
        {-1, 3,0,{255,255,255,91},{{.125f,.25f},{.375f,.25f},{.625f,.25f},{.875f,.25f}}}
    };
    auto *buffer = GxBufStream(GxPoolTarget_Vertex,sizeof(PCT4),3);
    auto *bytes = GxBufLock(buffer); std::memcpy(bytes,vertices,sizeof(vertices)); GxBufUnlock(buffer,0);
    CGxVertexAttrib format[] = {{GxVA_Position,4,0,48},{GxVA_Color0,0,12,48},
        {GxVA_TexCoord0,3,16,48},{GxVA_TexCoord1,3,24,48},
        {GxVA_TexCoord2,3,32,48},{GxVA_TexCoord3,3,40,48}};
    const uint16_t indices[] = {0,1,2,0,1,9};
    GxPrimIndexPtr(6,indices);
    auto geometry = [&] { device.PrimVertexFormat(buffer,format,6); device.PrimVertexMask(0x3D1); };
    CGxBatch batch{GxPrim_Triangles,0,3,0,2};
    const CImVector background(17,31,47,63);
    for (bool gaussian : {false,true}) {
        CGxShader *shaders[2]{};
        device.ShaderCreate(shaders,GxSh_Pixel,"Shaders\\Pixel",gaussian ? "FFXGauss4" : "FFXBox4",2);
        struct ShaderCleanup { LegacyDevice &d; CGxShader **s; ~ShaderCleanup() { d.ShaderDestroy(s[0]); d.ShaderDestroy(s[1]); } } shadersCleanup{device,shaders};
        GxRsSet(GxRs_PixelShader,shaders[0]);
        for (bool drawable : {false,true}) {
            device.DeviceSetRenderTarget(GxBuffers_Color,drawable ? nullptr : output,0);
            geometry();
            device.SceneClear(GxBufferBit_Color,background);
            device.Draw(&batch,1);
            const auto pixel = Pixel(device,drawable ? 160 : 24,drawable ? 120 : 16);
            std::array<double,4> expected{};
            for (unsigned i = 0; i < 4; ++i) for (unsigned c = 0; c < 4; ++c) {
                const unsigned shift = c == 3 ? 12 : 8-c*4;
                expected[c] += ((texels[i] >> shift) & 15)*17*(gaussian ? (i == 0 || i == 3 ? .125 : .375) : .25);
            }
            if (!gaussian) expected[3] = 91;
            const uint8_t actual[] = {pixel.r,pixel.g,pixel.b,pixel.a};
            for (unsigned c = 0; c < 4; ++c)
                Require(std::abs(actual[c]-expected[c]) <= 2, "FFX four-UV RGB/alpha equation mismatch");
        }
        device.DeviceSetRenderTarget(GxBuffers_Color,output,0);
        geometry(); device.SceneClear(GxBufferBit_Color,background);
        CGxBatch bad[] = {batch,{GxPrim_Triangles,3,3,0,9}};
        Reject([&] { device.Draw(bad,2); }, "FFX invalid later fetch accepted");
        auto unchanged = [&] { auto p = Pixel(device,24,16); Require(p.r == background.r && p.g == background.g &&
            p.b == background.b && p.a == background.a,"Rejected FFX draw partially painted output"); };
        unchanged();
        GxRsSet(GxRs_Texture3,other);
        Reject([&] { device.Draw(&batch,1); }, "FFX unequal fourth input accepted"); unchanged();
        GxRsSet(GxRs_Texture3,input);
        device.PrimVertexMask(0x1D1);
        Reject([&] { device.Draw(&batch,1); }, "FFX missing fourth UV accepted"); unchanged();
        geometry();
        // Original Gauss4 attaches its next output BEFORE replacing the old
        // logical input. A stale physical binding must not block that change;
        // a subsequent draw that actually samples the attachment must fail.
        device.DeviceSetRenderTarget(GxBuffers_Color,nullptr,0);
        for (unsigned i = 0; i < 4; ++i) GxRsSet(EGxRenderState(GxRs_Texture0+i),output);
        device.Draw(&batch,1);
        device.DeviceSetRenderTarget(GxBuffers_Color,output,0);
        device.SceneClear(GxBufferBit_Color,background);
        Reject([&] { device.Draw(&batch,1); }, "FFX attached input accepted"); unchanged();
        for (unsigned i = 0; i < 4; ++i) GxRsSet(EGxRenderState(GxRs_Texture0+i),input);
        device.Draw(&batch,1);
        Require(device.Caps().m_numTmus == 1 && device.Caps().m_shaderConstants[GxSh_Vertex] == 94,
                "FFX changed qualified device limits");
    }
    std::cout << "legacy-ffx-box-gauss-four-UV-RGBA-drawable-FBO-preflight-feedback-recovery=passed\n";
}
