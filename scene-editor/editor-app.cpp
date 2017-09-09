#include "index.hpp"
#include "editor-app.hpp"
#include "gui.hpp"
#include "serialization.hpp"

using namespace avl;

static int selectedTarget = 16;
static int arrayView = 0;
static float kUpsampleTolerance = +1.5;
static float kBlurTolerance = -4.6;
static float kNoiseStrength = 0.0;

static float RejectionFalloff = 1.f;
static float Accentuation = 0.1f;
static bool enableAo = true;

struct ScreenSpaceAmbientOcclusionPass
{
    // Controls how aggressive to fade off samples that occlude spheres but by so much as to be unreliable.
    // This is what gives objects a dark halo around them when placed in front of a wall.  If you want to
    // fade off the halo, boost your rejection falloff.  The tradeoff is that it reduces overall AO.
    //float RejectionFalloff = 1.f;  // 1.0f, 10.0f, 0.5f;

    // The effect normally marks anything that's 50% occluded or less as "fully unoccluded".  This throws away
    // half of our result.  Accentuation gives more range to the effect, but it will darken all AO values in the
    // process.  It will also cause "under occluded" geometry to appear to be highlighted.  If your ambient light
    // is determined by the surface normal (such as with IBL), you might not want this side effect.
    //float Accentuation = 0.1f; // 0.0f, 1.0f, 0.1f;

    GlTexture2D depthLinear;

    GlTexture2D lowDepth1; // L1 = /2
    GlTexture2D lowDepth2;
    GlTexture2D lowDepth3;
    GlTexture2D lowDepth4; // L4

    GlTexture3D tiledDepth1; // 3
    GlTexture3D tiledDepth2; // 4
    GlTexture3D tiledDepth3; // 5
    GlTexture3D tiledDepth4; // L6

    GlTexture2D occlusion1; // L1
    GlTexture2D occlusion2;
    GlTexture2D occlusion3;
    GlTexture2D occlusion4;

    GlTexture2D combined1; // L1
    GlTexture2D combined2;
    GlTexture2D combined3;

    GlTexture2D ambientOcclusion;

    //GlComputeProgram linearizeDepth = preprocess_compute_defines(read_file_text("../assets/shaders/ssao/linearize_depth_comp.glsl"), {});

    GlComputeProgram prepareDepth1 = preprocess_compute_defines(read_file_text("../assets/shaders/ssao/ao_prepare_depth1_comp.glsl"), {});
    GlComputeProgram prepareDepth2 = preprocess_compute_defines(read_file_text("../assets/shaders/ssao/ao_prepare_depth2_comp.glsl"), {});

    GlComputeProgram aoRender1 = preprocess_compute_defines(read_file_text("../assets/shaders/ssao/ao_render.glsl"), { "INTERLEAVE_RESULT" });
    //GlComputeProgram aoRender2 = preprocess_compute_defines(read_file_text("../assets/shaders/ssao/ao_render.glsl"), {});

    GlComputeProgram blendOut = preprocess_compute_defines(read_file_text("../assets/shaders/ssao/ao_blur_and_upsample_comp.glsl"), { "BLEND_WITH_HIGHER_RESOLUTION" });
    //GlComputeProgram preMinBlendOut = preprocess_compute_defines(read_file_text("../assets/shaders/ssao/ao_blur_and_upsample_comp.glsl"), { "COMBINE_LOWER_RESOLUTIONS", "BLEND_WITH_HIGHER_RESOLUTION" });
    GlComputeProgram upsample = preprocess_compute_defines(read_file_text("../assets/shaders/ssao/ao_blur_and_upsample_comp.glsl"), {});
    //GlComputeProgram PreMin = preprocess_compute_defines(read_file_text("../assets/shaders/ssao/ao_blur_and_upsample_comp.glsl"), { "COMBINE_LOWER_RESOLUTIONS" });

    std::vector<float4> SampleThickness; // Pre-computed sample thicknesses
    std::vector<float4> InvThicknessTable;
    std::vector<float4> SampleWeightTable;

    float CalculateTanHalfFovHeight(const float4x4 & projectionMatrix)
    {
        return 1.f / projectionMatrix[0][0];
    }

    // Calculate width/height of the texture from the base dimensions.
    int2 CalculateMipSize(float2 size, int level)
    {
        auto div = 1 << (int) level;
        size.x = (size.x + (div - 1)) / div;
        size.y = (size.y + (div - 1)) / div;
        return int2(size);
    }

    /*
    
        g_DepthDownsize1.Create( L"Depth Down-Sized 1", bufferWidth1, bufferHeight1, 1, DXGI_FORMAT_R32_FLOAT, esram );
        g_DepthDownsize2.Create( L"Depth Down-Sized 2", bufferWidth2, bufferHeight2, 1, DXGI_FORMAT_R32_FLOAT, esram );
        g_DepthDownsize3.Create( L"Depth Down-Sized 3", bufferWidth3, bufferHeight3, 1, DXGI_FORMAT_R32_FLOAT, esram );
        g_DepthDownsize4.Create( L"Depth Down-Sized 4", bufferWidth4, bufferHeight4, 1, DXGI_FORMAT_R32_FLOAT, esram );
        g_DepthTiled1.CreateArray( L"Depth De-Interleaved 1", bufferWidth3, bufferHeight3, 16, DXGI_FORMAT_R16_FLOAT, esram );
        g_DepthTiled2.CreateArray( L"Depth De-Interleaved 2", bufferWidth4, bufferHeight4, 16, DXGI_FORMAT_R16_FLOAT, esram );
        g_DepthTiled3.CreateArray( L"Depth De-Interleaved 3", bufferWidth5, bufferHeight5, 16, DXGI_FORMAT_R16_FLOAT, esram );
        g_DepthTiled4.CreateArray( L"Depth De-Interleaved 4", bufferWidth6, bufferHeight6, 16, DXGI_FORMAT_R16_FLOAT, esram );

        // Occlusion...
        g_AOMerged1.Create( L"AO Re-Interleaved 1", bufferWidth1, bufferHeight1, 1, DXGI_FORMAT_R8_UNORM, esram );
        g_AOMerged2.Create( L"AO Re-Interleaved 2", bufferWidth2, bufferHeight2, 1, DXGI_FORMAT_R8_UNORM, esram );
        g_AOMerged3.Create( L"AO Re-Interleaved 3", bufferWidth3, bufferHeight3, 1, DXGI_FORMAT_R8_UNORM, esram );
        g_AOMerged4.Create( L"AO Re-Interleaved 4", bufferWidth4, bufferHeight4, 1, DXGI_FORMAT_R8_UNORM, esram );

        // Combined?
        g_AOSmooth1.Create( L"AO Smoothed 1", bufferWidth1, bufferHeight1, 1, DXGI_FORMAT_R8_UNORM, esram );
        g_AOSmooth2.Create( L"AO Smoothed 2", bufferWidth2, bufferHeight2, 1, DXGI_FORMAT_R8_UNORM, esram );
        g_AOSmooth3.Create( L"AO Smoothed 3", bufferWidth3, bufferHeight3, 1, DXGI_FORMAT_R8_UNORM, esram );

        g_AOHighQuality1.Create( L"AO High Quality 1", bufferWidth1, bufferHeight1, 1, DXGI_FORMAT_R8_UNORM, esram );
        g_AOHighQuality2.Create( L"AO High Quality 2", bufferWidth2, bufferHeight2, 1, DXGI_FORMAT_R8_UNORM, esram );
        g_AOHighQuality3.Create( L"AO High Quality 3", bufferWidth3, bufferHeight3, 1, DXGI_FORMAT_R8_UNORM, esram );
        g_AOHighQuality4.Create( L"AO High Quality 4", bufferWidth4, bufferHeight4, 1, DXGI_FORMAT_R8_UNORM, esram );
    */

    ScreenSpaceAmbientOcclusionPass()
    {
        const float2 renderSize = { 1920, 1080 };

        SampleThickness.resize(3);
        InvThicknessTable.resize(3);
        SampleWeightTable.resize(3);

        depthLinear.setup(renderSize.x, renderSize.y, GL_R32F, GL_RED, GL_FLOAT, nullptr);
        ambientOcclusion.setup(renderSize.x, renderSize.y, GL_R32F, GL_RED, GL_FLOAT, nullptr);

        const float2 renderSize1 = float2((renderSize.x + 1) / 2, (renderSize.y + 1) / 2);
        const float2 renderSize2 = float2((renderSize.x + 3) / 4, (renderSize.y + 3) / 4);
        const float2 renderSize3 = float2((renderSize.x + 7) / 8, (renderSize.y + 7) / 8);
        const float2 renderSize4 = float2((renderSize.x + 15) / 16, (renderSize.y + 15) / 16);
        const float2 renderSize5 = float2((renderSize.x + 31) / 32, (renderSize.y + 31) / 32);
        const float2 renderSize6 = float2((renderSize.x + 63) / 64, (renderSize.y + 63) / 64);

        {
            lowDepth1.setup(renderSize1.x, renderSize1.y, GL_R32F, GL_RED, GL_FLOAT, nullptr);
            lowDepth2.setup(renderSize2.x, renderSize2.y, GL_R32F, GL_RED, GL_FLOAT, nullptr);
            lowDepth3.setup(renderSize3.x, renderSize3.y, GL_R32F, GL_RED, GL_FLOAT, nullptr);
            lowDepth4.setup(renderSize4.x, renderSize4.y, GL_R32F, GL_RED, GL_FLOAT, nullptr);
        }

        {
            tiledDepth1.setup(GL_TEXTURE_2D_ARRAY, renderSize3.x, renderSize3.y, 16, GL_R32F, GL_RED, GL_FLOAT, nullptr); // 3
            tiledDepth2.setup(GL_TEXTURE_2D_ARRAY, renderSize4.x, renderSize4.y, 16, GL_R32F, GL_RED, GL_FLOAT, nullptr); // 4
            tiledDepth3.setup(GL_TEXTURE_2D_ARRAY, renderSize5.x, renderSize5.y, 16, GL_R32F, GL_RED, GL_FLOAT, nullptr); // 5
            tiledDepth4.setup(GL_TEXTURE_2D_ARRAY, renderSize6.x, renderSize6.y, 16, GL_R32F, GL_RED, GL_FLOAT, nullptr); // 6
        }

        {
            occlusion1.setup(renderSize1.x, renderSize1.y, GL_R32F, GL_RED, GL_FLOAT, nullptr);
            occlusion2.setup(renderSize2.x, renderSize2.y, GL_R32F, GL_RED, GL_FLOAT, nullptr);
            occlusion3.setup(renderSize3.x, renderSize3.y, GL_R32F, GL_RED, GL_FLOAT, nullptr);
            occlusion4.setup(renderSize4.x, renderSize4.y, GL_R32F, GL_RED, GL_FLOAT, nullptr);
        }

        {
            combined1.setup(renderSize1.x, renderSize1.y, GL_R32F, GL_RED, GL_FLOAT, nullptr);
            combined2.setup(renderSize2.x, renderSize2.y, GL_R32F, GL_RED, GL_FLOAT, nullptr);
            combined3.setup(renderSize3.x, renderSize3.y, GL_R32F, GL_RED, GL_FLOAT, nullptr);
        }

        SampleThickness[0].x = sqrt(1.0f - 0.2f * 0.2f);
        SampleThickness[0].y = sqrt(1.0f - 0.4f * 0.4f);
        SampleThickness[0].z = sqrt(1.0f - 0.6f * 0.6f);
        SampleThickness[0].w = sqrt(1.0f - 0.8f * 0.8f);

        SampleThickness[1].x = sqrt(1.0f - 0.2f * 0.2f - 0.2f * 0.2f);
        SampleThickness[1].y = sqrt(1.0f - 0.2f * 0.2f - 0.4f * 0.4f);
        SampleThickness[1].z = sqrt(1.0f - 0.2f * 0.2f - 0.6f * 0.6f);
        SampleThickness[1].w = sqrt(1.0f - 0.2f * 0.2f - 0.8f * 0.8f);

        SampleThickness[2].x = sqrt(1.0f - 0.4f * 0.4f - 0.4f * 0.4f);
        SampleThickness[2].y = sqrt(1.0f - 0.4f * 0.4f - 0.6f * 0.6f);
        SampleThickness[2].z = sqrt(1.0f - 0.4f * 0.4f - 0.8f * 0.8f);
        SampleThickness[2].w = sqrt(1.0f - 0.6f * 0.6f - 0.6f * 0.6f);
    } 
    
    void PushDownsampleCommands(const float2 nearFarClip, const GLuint depthSource)
    {
        // First downsampling pass
        {
            glUseProgram(prepareDepth1.handle());
            glBindImageTexture(0, depthLinear, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
            glBindImageTexture(1, lowDepth1, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
            glBindImageTexture(2, tiledDepth1, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_R32F);
            glBindImageTexture(3, lowDepth2, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
            glBindImageTexture(4, tiledDepth2, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_R32F);
            prepareDepth1.texture("Depth", 0, depthSource, GL_TEXTURE_2D); // sampler 
            prepareDepth1.dispatch(tiledDepth2.width * 8, tiledDepth2.height * 8, 1);
        }

        // Second downsampling pass
        {
            glUseProgram(prepareDepth2.handle());
            glBindImageTexture(0, lowDepth3, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
            glBindImageTexture(1, tiledDepth3, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_R32F);
            glBindImageTexture(2, lowDepth4, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
            glBindImageTexture(3, tiledDepth4, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_R32F);
            prepareDepth2.texture("DS4x", 0, lowDepth2, GL_TEXTURE_2D); // sampler 
            prepareDepth2.dispatch(tiledDepth4.width, tiledDepth4.height, 1);
        }
    }

    void PushRenderAOCommands(const GlTexture3D & source, const GlTexture2D & dest, const float TanHalfFovH)
    {
        // The shaders are set up to sample a circular region within a 5-pixel radius.
        const float ScreenspaceDiameter = 5.0f;
        float ThicknessMultiplier = 2.0f * TanHalfFovH * ScreenspaceDiameter / source.width;

        if (source.depth == 1) ThicknessMultiplier *= 2.0f;

        float InverseRangeFactor = 1.0f / ThicknessMultiplier;

        for (int i = 0; i < 3; i++)
        {
            InvThicknessTable[i].x = InverseRangeFactor / SampleThickness[i].x; // hmm
            InvThicknessTable[i].y = InverseRangeFactor / SampleThickness[i].y; // hmm
            InvThicknessTable[i].z = InverseRangeFactor / SampleThickness[i].z; // hmm
            InvThicknessTable[i].w = InverseRangeFactor / SampleThickness[i].w; // hmm
        }

        SampleWeightTable[0].x = 4 * SampleThickness[0].x;   // Axial
        SampleWeightTable[0].y = 4 * SampleThickness[0].y;   // Axial
        SampleWeightTable[0].z = 4 * SampleThickness[0].z;   // Axial
        SampleWeightTable[0].w = 4 * SampleThickness[0].w;   // Axial
                                                         
        SampleWeightTable[1].x = 4 * SampleThickness[1].x;   // Diagonal
        SampleWeightTable[1].y = 8 * SampleThickness[1].y;   // L-shaped
        SampleWeightTable[1].z = 8 * SampleThickness[1].z;   // L-shaped
        SampleWeightTable[1].w = 8 * SampleThickness[1].w;   // L-shaped
                                                         
        SampleWeightTable[2].x = 4 * SampleThickness[2].x;   // Diagonal
        SampleWeightTable[2].y = 8 * SampleThickness[2].y;   // L-shaped
        SampleWeightTable[2].z = 8 * SampleThickness[2].z;   // L-shaped
        SampleWeightTable[2].w = 4 * SampleThickness[2].w;   // Diagonal

        // Normalize the weights by dividing by the sum of all weights
        float totalWeight = 0.f;
        for (auto & w : SampleWeightTable)
        {
            totalWeight += w.x;
            totalWeight += w.y;
            totalWeight += w.z;
            totalWeight += w.w;
        }

        for (int i = 0; i < 3; ++i) SampleWeightTable[i] /= totalWeight;

        glUseProgram(aoRender1.handle());
        aoRender1.uniform("gInvThicknessTable", 3, InvThicknessTable);
        aoRender1.uniform("gSampleWeightTable", 3, SampleWeightTable);
        aoRender1.uniform("gInvSliceDimension", float2(1.0f / source.width, 1.0f / source.height));
        aoRender1.uniform("gRejectFadeoff", 1.f / -RejectionFalloff);
        aoRender1.uniform("gRcpAccentuation", 1.0f / (1.0f + Accentuation));

        aoRender1.texture("DepthTex", 0, source, GL_TEXTURE_2D_ARRAY);
        glBindImageTexture(0, dest, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);

        auto workgroupSize = uint3(8, 8, 1); // thread local
        workgroupSize.x = (source.width +  (int) workgroupSize.x - 1) / (int) workgroupSize.x;
        workgroupSize.y = (source.height + (int) workgroupSize.y - 1) / (int) workgroupSize.y;
        workgroupSize.z = (source.depth +  (int) workgroupSize.z - 1) / (int) workgroupSize.z;
        aoRender1.dispatch(workgroupSize);

        gl_check_error(__FILE__, __LINE__);
    }


    void PushUpsampleCommands(const GlTexture2D & lowResDepth, const GlTexture2D & highResDepth, const GlTexture2D & lowResAO, const GlTexture2D & highResAO, const GlTexture2D & dest, bool highRes)
    {
        GlComputeProgram & prog = (highRes == true) ? blendOut : upsample;

        glUseProgram(prog.handle());

        const float stepSize = 1000.f / lowResDepth.width; // fix hardcoded resolution
        float blurTolerance = (1.0 - std::pow(10,  kBlurTolerance) * stepSize);
        blurTolerance *= blurTolerance;
        const float upsampleTolerance = std::pow(10, kUpsampleTolerance);
        const float noiseFilterWeight = 1.0f / (std::pow(10, kNoiseStrength) + upsampleTolerance); // noise filter tolerance

        prog.uniform("InvLowResolution", float2(1.f / lowResDepth.width, 1.f / lowResDepth.height));
        prog.uniform("InvHighResolution", float2(1.f / highResDepth.width, 1.f / highResDepth.height));
        prog.uniform("NoiseFilterStrength", noiseFilterWeight);
        prog.uniform("StepSize", stepSize);
        prog.uniform("kBlurTolerance", blurTolerance);
        prog.uniform("kUpsampleTolerance", upsampleTolerance);

        prog.texture("LoResDB", 0, lowResDepth, GL_TEXTURE_2D);
        prog.texture("HiResDB", 1, highResDepth, GL_TEXTURE_2D);
        prog.texture("LoResAO1", 2, lowResAO, GL_TEXTURE_2D);

        // BLEND_WITH_HIGHER_RESOLUTION is the blendOut program
        if (highRes == true) prog.texture("HiResAO", 3, highResAO, GL_TEXTURE_2D);

        glBindImageTexture(0, dest, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F); // AoResult

        int xcount = (highResDepth.width + 17) / 16;
        int ycount = (highResDepth.height + 17) / 16;

        prog.dispatch(xcount, ycount, 1);
    }

    void RebuildCommandBuffers(const float4x4 & projectionMatrix, const GLuint depthSrc)
    {
        const float tanHalfFovH = CalculateTanHalfFovHeight(projectionMatrix);
        const float2 nearFarClip = near_far_clip_from_projection(projectionMatrix);

        // Phase 1: Decompress, linearize, downsample, and deinterleave the depth buffer
        PushDownsampleCommands(nearFarClip, depthSrc);

        // Phase 2: Render SSAO for each sub-tile
        PushRenderAOCommands(tiledDepth4, occlusion4, tanHalfFovH);
        PushRenderAOCommands(tiledDepth3, occlusion3, tanHalfFovH);
        PushRenderAOCommands(tiledDepth2, occlusion2, tanHalfFovH);
        PushRenderAOCommands(tiledDepth1, occlusion1, tanHalfFovH);

        // Phase 3: Iteratively blur and upsample, combining each result
        PushUpsampleCommands(lowDepth4, lowDepth3, occlusion4, occlusion3, combined3, true);
        PushUpsampleCommands(lowDepth3, lowDepth2, occlusion3, occlusion2, combined2, true);
        PushUpsampleCommands(lowDepth2, lowDepth1, occlusion2, occlusion1, combined1, true);
        PushUpsampleCommands(lowDepth1, depthLinear, combined1, combined1, ambientOcclusion, false);
    }

};

std::unique_ptr<ScreenSpaceAmbientOcclusionPass> ssao;

scene_editor_app::scene_editor_app() : GLFWApp(1920, 1080, "Scene Editor")
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    igm.reset(new gui::ImGuiManager(window));
    gui::make_dark_theme();

    editor.reset(new editor_controller<GameObject>());

    ssaoDebugView.reset(new GLTextureView(true));
    ssaoArrayView.reset(new GLTextureView3D());

    cam.look_at({ 0, 9.5f, -6.0f }, { 0, 0.1f, 0 });
    flycam.set_camera(&cam);

    scene.skybox.reset(new HosekProceduralSky());

    auto wireframeProgram = GlShader(
        read_file_text("../assets/shaders/wireframe_vert.glsl"), 
        read_file_text("../assets/shaders/wireframe_frag.glsl"),
        read_file_text("../assets/shaders/wireframe_geom.glsl"));
    global_register_asset("wireframe", std::move(wireframeProgram));

     // USE_IMAGE_BASED_LIGHTING
    shaderMonitor.watch(
        "../assets/shaders/renderer/forward_lighting_vert.glsl", 
        "../assets/shaders/renderer/forward_lighting_frag.glsl", 
        "../assets/shaders/renderer", 
        {"TWO_CASCADES", "USE_PCF_3X3", 
         "USE_IMAGE_BASED_LIGHTING", 
         "HAS_ROUGHNESS_MAP", "HAS_METALNESS_MAP", "HAS_ALBEDO_MAP", "HAS_NORMAL_MAP", "HAS_OCCLUSION_MAP"}, [](GlShader shader)
    {
        auto & asset = AssetHandle<GlShader>("pbr-forward-lighting").assign(std::move(shader));
    });

    shaderMonitor.watch(
        "../assets/shaders/renderer/shadowcascade_vert.glsl", 
        "../assets/shaders/renderer/shadowcascade_frag.glsl", 
        "../assets/shaders/renderer/shadowcascade_geom.glsl", 
        "../assets/shaders/renderer", {}, 
        [](GlShader shader) {
        auto & asset = AssetHandle<GlShader>("cascaded-shadows").assign(std::move(shader));
    });

    shaderMonitor.watch(
        "../assets/shaders/renderer/post_tonemap_vert.glsl",
        "../assets/shaders/renderer/post_tonemap_frag.glsl",
        [](GlShader shader) {
        auto & asset = AssetHandle<GlShader>("post-tonemap").assign(std::move(shader));
    });

    renderer.reset(new PhysicallyBasedRenderer<1>(float2(width, height)));
    renderer->set_procedural_sky(scene.skybox.get());

    auto radianceBinary = read_file_binary("../assets/textures/envmaps/wells_radiance.dds");
    auto irradianceBinary = read_file_binary("../assets/textures/envmaps/wells_irradiance.dds");
    gli::texture_cube radianceHandle(gli::load_dds((char *)radianceBinary.data(), radianceBinary.size()));
    gli::texture_cube irradianceHandle(gli::load_dds((char *)irradianceBinary.data(), irradianceBinary.size()));
    global_register_asset("wells-radiance-cubemap", load_cubemap(radianceHandle));
    global_register_asset("wells-irradiance-cubemap", load_cubemap(irradianceHandle));

    global_register_asset("rusted-iron-albedo", load_image("../assets/nonfree/Metal_ModernMetalIsoDiamondTile_2k_basecolor.tga", false));
    global_register_asset("rusted-iron-normal", load_image("../assets/nonfree/Metal_ModernMetalIsoDiamondTile_2k_n.tga", false));
    global_register_asset("rusted-iron-metallic", load_image("../assets/nonfree/Metal_ModernMetalIsoDiamondTile_2k_metallic.tga", false));
    global_register_asset("rusted-iron-roughness", load_image("../assets/nonfree/Metal_ModernMetalIsoDiamondTile_2k_roughness.tga", false));
    global_register_asset("rusted-iron-occlusion", load_image("../assets/nonfree/Metal_ModernMetalIsoDiamondTile_2k_ao.tga", false));

    global_register_asset("scifi-floor-albedo", load_image("../assets/nonfree/Metal_ScifiHangarFloor_2k_basecolor.tga", false));
    global_register_asset("scifi-floor-normal", load_image("../assets/nonfree/Metal_ScifiHangarFloor_2k_n.tga", false));
    global_register_asset("scifi-floor-metallic", load_image("../assets/nonfree/Metal_ScifiHangarFloor_2k_metallic.tga", false));
    global_register_asset("scifi-floor-roughness", load_image("../assets/nonfree/Metal_ScifiHangarFloor_2k_roughness.tga", false));
    global_register_asset("scifi-floor-occlusion", load_image("../assets/nonfree/Metal_ScifiHangarFloor_2k_ao.tga", false));

    //auto shaderball = load_geometry_from_ply("../assets/models/shaderball/shaderball.ply");
    auto shaderball = load_geometry_from_ply("../assets/models/geometry/CubeHollowOpen.ply");
    rescale_geometry(shaderball, 1.f);
    global_register_asset("shaderball", make_mesh_from_geometry(shaderball));
    global_register_asset("shaderball", std::move(shaderball));

    auto ico = make_icosasphere(5);
    global_register_asset("icosphere", make_mesh_from_geometry(ico));
    global_register_asset("icosphere", std::move(ico));

    MetallicRoughnessMaterial floorInstance;
    floorInstance.program = GlShaderHandle("pbr-forward-lighting");
    floorInstance.roughnessFactor = 0.5;
    floorInstance.metallicFactor = 0.88f;
    floorInstance.albedo = GlTextureHandle("scifi-floor-albedo");
    floorInstance.normal = GlTextureHandle("scifi-floor-normal");
    floorInstance.metallic = GlTextureHandle("scifi-floor-metallic");
    floorInstance.roughness = GlTextureHandle("scifi-floor-roughness");
    floorInstance.occlusion = GlTextureHandle("scifi-floor-occlusion");
    floorInstance.radianceCubemap = GlTextureHandle("wells-radiance-cubemap");
    floorInstance.irradianceCubemap = GlTextureHandle("wells-irradiance-cubemap");

    /*
    std::shared_ptr<MetallicRoughnessMaterial> rustedInstance;
    rustedInstance = std::make_shared<MetallicRoughnessMaterial>();
    rustedInstance->program = GlShaderHandle("pbr-forward-lighting");
    rustedInstance->albedo = GlTextureHandle("rusted-iron-albedo");
    rustedInstance->normal = GlTextureHandle("rusted-iron-normal");
    rustedInstance->metallic = GlTextureHandle("rusted-iron-metallic");
    rustedInstance->roughness = GlTextureHandle("rusted-iron-roughness");
    rustedInstance->height = GlTextureHandle("rusted-iron-height");
    rustedInstance->occlusion = GlTextureHandle("rusted-iron-occlusion");
    rustedInstance->radianceCubemap = GlTextureHandle("wells-radiance-cubemap");
    rustedInstance->irradianceCubemap = GlTextureHandle("wells-irradiance-cubemap");
    global_register_asset("pbr-material/floor", static_cast<std::shared_ptr<Material>>(rustedInstance));
    */

    std::shared_ptr<MetallicRoughnessMaterial> rustedInstance;
    cereal::deserialize_from_json("rust.mat.json", rustedInstance);
    scene.materialInstances["pbr-material/floor"] = rustedInstance;

    for (int i = 0; i < 6; ++i)
    {
        for (int j = 0; j < 6; ++j)
        {
            //serialize_test(pbrMaterialInstance);

            std::shared_ptr<MetallicRoughnessMaterial> instance{ new MetallicRoughnessMaterial(*rustedInstance) };
            instance->roughnessFactor = remap<float>(i, 0.0f, 5.0f, 0.0f, 1.f);
            instance->metallicFactor = remap<float>(j, 0.0f, 5.0f, 0.0f, 1.f);
            const std::string material_id = "pbr-material/" + std::to_string(i) + "-" + std::to_string(j);
            scene.materialInstances[material_id] = instance;


            StaticMesh m;
            Pose p;
            p.position = float3((i * 3) - 5, 0, (j * 3) - 5);
            m.set_pose(p);
            m.set_material(AssetHandle<std::shared_ptr<Material>>(material_id));
            m.mesh = GlMeshHandle("shaderball");
            m.geom = GeometryHandle("shaderball");
            m.receive_shadow = true;
            scene.objects.push_back(std::make_shared<StaticMesh>(std::move(m)));
        }
    }

    auto cube = make_cube();
    global_register_asset("cube", make_mesh_from_geometry(cube));
    global_register_asset("cube", std::move(cube));

    StaticMesh floorMesh;
    floorMesh.mesh = GlMeshHandle("cube");
    floorMesh.geom = GeometryHandle("cube");
    floorMesh.set_pose(Pose(float3(0, .01f, 0)));
    floorMesh.set_scale(float3(16, 0.1f, 16));
    floorMesh.set_material("pbr-material/floor");
    std::shared_ptr<StaticMesh> floor = std::make_shared<StaticMesh>(std::move(floorMesh));
    scene.objects.push_back(floor);

    scene.lightA.reset(new PointLight());
    scene.lightA->data.color = float3(0.88f, 0.85f, 0.97f);
    scene.lightA->data.position = float3(-6.0, 3.0, 0);
    scene.lightA->data.radius = 4.f;
    scene.objects.push_back(scene.lightA);

    scene.lightB.reset(new PointLight());
    scene.lightB->data.color = float3(0.67f, 1.00f, 0.85f);
    scene.lightB->data.position = float3(+6, 3.0, 0);
    scene.lightB->data.radius = 4.f;
    scene.objects.push_back(scene.lightB);

    // Register all material instances with the asset system. Since everything is handle-based,
    // we can do this wherever, so long as it's before the first rendered frame
    for (auto & instance : scene.materialInstances)
    {
        global_register_asset_shared(instance.first.c_str(), static_cast<std::shared_ptr<Material>>(instance.second));
    }

    aoBlit = GlShader(read_file_text("../assets/shaders/renderer/post_tonemap_vert.glsl"), read_file_text("../assets/shaders/renderer/ao_blit_frag.glsl"));
    fullscreenquad = make_fullscreen_quad();
    ssao.reset(new ScreenSpaceAmbientOcclusionPass());
}

scene_editor_app::~scene_editor_app()
{ 

}

void scene_editor_app::on_window_resize(int2 size) 
{ 

}

void scene_editor_app::on_input(const InputEvent & event)
{
    igm->update_input(event);
    flycam.handle_input(event);
    editor->on_input(event);

    // Prevent scene editor from responding to input destined for ImGui
    if (!ImGui::GetIO().WantCaptureMouse && !ImGui::GetIO().WantCaptureKeyboard)
    {

        if (event.type == InputEvent::KEY)
        {
            if (event.value[0] == GLFW_KEY_ESCAPE && event.action == GLFW_RELEASE)
            {
                // De-select all objects
                editor->clear();
            }
        }

        if (event.type == InputEvent::MOUSE && event.action == GLFW_PRESS && event.value[0] == GLFW_MOUSE_BUTTON_LEFT)
        {
            int width, height;
            glfwGetWindowSize(window, &width, &height);

            const Ray r = cam.get_world_ray(event.cursor, float2(width, height));

            if (length(r.direction) > 0 && !editor->active())
            {
                std::vector<GameObject *> selectedObjects;
                float best_t = std::numeric_limits<float>::max();
                GameObject * hitObject = nullptr;

                for (auto & obj : scene.objects)
                {
                    RaycastResult result = obj->raycast(r);
                    if (result.hit)
                    {
                        if (result.distance < best_t)
                        {
                            best_t = result.distance;
                            hitObject = obj.get();
                        }
                    }
                }

                if (hitObject) selectedObjects.push_back(hitObject);

                // New object was selected
                if (selectedObjects.size() > 0)
                {
                    // Multi-selection
                    if (event.mods & GLFW_MOD_CONTROL)
                    {
                        auto existingSelection = editor->get_selection();
                        for (auto s : selectedObjects)
                        {
                            if (!editor->selected(s)) existingSelection.push_back(s);
                        }
                        editor->set_selection(existingSelection);
                    }
                    // Single Selection
                    else
                    {
                        editor->set_selection(selectedObjects);
                    }
                }
            }
        }
    }

}

void scene_editor_app::on_update(const UpdateEvent & e)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);

    flycam.update(e.timestep_ms);
    shaderMonitor.handle_recompile();
    editor->on_update(cam, float2(width, height));
}

void scene_editor_app::on_draw()
{
    glfwMakeContextCurrent(window);

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    glViewport(0, 0, width, height);
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const float4x4 projectionMatrix = cam.get_projection_matrix(float(width) / float(height));
    const float4x4 viewMatrix = cam.get_view_matrix();
    const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);

    {
        // Single-viewport camera
        CameraData renderCam;
        renderCam.index = 0;
        renderCam.pose = cam.get_pose();
        renderCam.projectionMatrix = projectionMatrix;
        renderCam.viewMatrix = viewMatrix;
        renderCam.viewProjMatrix = viewProjectionMatrix;
        renderer->add_camera(renderCam);

        // Lighting
        renderer->add_light(&scene.lightA->data);
        renderer->add_light(&scene.lightB->data);

        // Gather Objects
        std::vector<Renderable *> sceneObjects;
        for (auto & obj : scene.objects)
        {
            if (auto * r = dynamic_cast<Renderable*>(obj.get())) sceneObjects.push_back(r);
        }
        renderer->add_objects(sceneObjects);

        renderer->render_frame();

        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);

        if (enableAo)
        {
            computeTimer.start();
            ssao->RebuildCommandBuffers(projectionMatrix, renderer->get_output_texture_depth(0));
            computeTimer.stop();

            Bounds2D renderArea = { 300.f, 0.f, (float)1200, (float)700 };

            switch (selectedTarget)
            {
                case 0:     ssaoDebugView->draw(renderArea, float2(width, height), ssao->depthLinear); break;
                case 1:     ssaoDebugView->draw(renderArea, float2(width, height), ssao->lowDepth1); break;
                case 2:     ssaoDebugView->draw(renderArea, float2(width, height), ssao->lowDepth2); break;
                case 3:     ssaoDebugView->draw(renderArea, float2(width, height), ssao->lowDepth3); break;
                case 4:     ssaoDebugView->draw(renderArea, float2(width, height), ssao->lowDepth4); break;
                case 9:     ssaoDebugView->draw(renderArea, float2(width, height), ssao->occlusion1); break;
                case 10:    ssaoDebugView->draw(renderArea, float2(width, height), ssao->occlusion2); break;
                case 11:    ssaoDebugView->draw(renderArea, float2(width, height), ssao->occlusion3); break;
                case 12:    ssaoDebugView->draw(renderArea, float2(width, height), ssao->occlusion4); break;
                case 13:    ssaoDebugView->draw(renderArea, float2(width, height), ssao->combined1); break;
                case 14:    ssaoDebugView->draw(renderArea, float2(width, height), ssao->combined2); break;
                case 15:    ssaoDebugView->draw(renderArea, float2(width, height), ssao->combined3); break;
                case 16:    ssaoDebugView->draw(renderArea, float2(width, height), ssao->ambientOcclusion); break;
            }

            switch (selectedTarget)
            {
                case 5:     ssaoArrayView->draw(renderArea, float2(width, height), ssao->tiledDepth1, GL_TEXTURE_2D_ARRAY, arrayView); break;
                case 6:     ssaoArrayView->draw(renderArea, float2(width, height), ssao->tiledDepth2, GL_TEXTURE_2D_ARRAY, arrayView); break;
                case 7:     ssaoArrayView->draw(renderArea, float2(width, height), ssao->tiledDepth3, GL_TEXTURE_2D_ARRAY, arrayView); break;
                case 8:     ssaoArrayView->draw(renderArea, float2(width, height), ssao->tiledDepth4, GL_TEXTURE_2D_ARRAY, arrayView); break;
            }

            glEnable(GL_BLEND);

            aoBlit.bind();
            aoBlit.texture("s_scene", 0, renderer->get_output_texture(0), GL_TEXTURE_2D);
            aoBlit.texture("s_ao", 1, ssao->ambientOcclusion, GL_TEXTURE_2D);
            fullscreenquad.draw_elements();
            aoBlit.unbind();

            glDisable(GL_BLEND);
        }
        else
        {
            glActiveTexture(GL_TEXTURE0);
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, renderer->get_output_texture(0));
            glBegin(GL_QUADS);
            glTexCoord2f(0, 0); glVertex2f(-1, -1);
            glTexCoord2f(1, 0); glVertex2f(+1, -1);
            glTexCoord2f(1, 1); glVertex2f(+1, +1);
            glTexCoord2f(0, 1); glVertex2f(-1, +1);
            glEnd();
            glDisable(GL_TEXTURE_2D);
        }


        gl_check_error(__FILE__, __LINE__);
    }


    // Selected objects as wireframe
    {
        glDisable(GL_DEPTH_TEST);

        auto & program = GlShaderHandle("wireframe").get();

        program.bind();

        program.uniform("u_eyePos", cam.get_eye_point());
        program.uniform("u_viewProjMatrix", viewProjectionMatrix);

        for (auto obj : editor->get_selection())
        {
            auto modelMatrix = mul(obj->get_pose().matrix(), make_scaling_matrix(obj->get_scale()));
            program.uniform("u_modelMatrix", modelMatrix);
            obj->draw();
        }

        program.unbind();

        glEnable(GL_DEPTH_TEST);
    }

    igm->begin_frame();

    {
        std::vector<std::string> targets = { "linear depth input", "low1", "low2", "low3", "low4", "td1", "td2", "td3", "td4", "oc1", "oc2", "oc3", "oc4", "comb1", "comb2", "comb3", "ao output" };
        ImGui::Combo("Targets", &selectedTarget, targets);
        ImGui::SliderInt("Array", &arrayView, 0, 16);
        ImGui::SliderFloat("Upsample Tolerance", &kUpsampleTolerance, -12.f, 12.f);
        ImGui::SliderFloat("Blur Tolerance", &kBlurTolerance, -8.f, 4.f);
        ImGui::SliderFloat("Denoise", &kNoiseStrength, -8.f, 0.f);
        ImGui::SliderFloat("RejectionFalloff", &RejectionFalloff, 1.0, 10.0f);
        ImGui::SliderFloat("Accentuation", &Accentuation, 0.0f, 1.f);
        
    }

    gui::imgui_menu_stack menu(*this, ImGui::GetIO().KeysDown);
    menu.app_menu_begin();
    {
        menu.begin("File");
        if (menu.item("Open Scene", GLFW_MOD_CONTROL, GLFW_KEY_O)) {}
        if (menu.item("Save Scene", GLFW_MOD_CONTROL, GLFW_KEY_S)) {}
        if (menu.item("New Scene", GLFW_MOD_CONTROL, GLFW_KEY_N)) {}
        if (menu.item("Exit", GLFW_MOD_ALT, GLFW_KEY_F4)) exit();
        menu.end();

        menu.begin("Edit");
        if (menu.item("Clone", GLFW_MOD_CONTROL, GLFW_KEY_D)) {}
        if (menu.item("Delete", 0, GLFW_KEY_DELETE)) 
        {
            auto it = std::remove_if(std::begin(scene.objects), std::end(scene.objects), [this](std::shared_ptr<GameObject> obj) 
            { 
                return editor->selected(obj.get());
            });
            scene.objects.erase(it, std::end(scene.objects));

            editor->clear();
        }
        if (menu.item("Select All", GLFW_MOD_CONTROL, GLFW_KEY_A)) 
        {
            std::vector<GameObject *> selectedObjects;
            for (auto & obj : scene.objects)
            {
                selectedObjects.push_back(obj.get());
            }
            editor->set_selection(selectedObjects);
        }
        menu.end();
    }
    menu.app_menu_end();

    static int horizSplit = 380;
    static int rightSplit1 = (height / 3) - 17;
    static int rightSplit2 = ((height / 3) - 17);
    
    // Define a split region between the whole window and the right panel
    auto rightRegion = ImGui::Split({ { 0.f,17.f },{ (float) width, (float) height } }, &horizSplit, ImGui::SplitType::Right);

    auto split2 = ImGui::Split(rightRegion.second, &rightSplit1, ImGui::SplitType::Top);
    auto split3 = ImGui::Split(split2.first, &rightSplit2, ImGui::SplitType::Top); // split again by the same amount

    ui_rect topRightPane = { { int2(split2.second.min()) }, { int2(split2.second.max()) } }; // top 1/3rd and `the rest`
    ui_rect middleRightPane = { { int2(split3.first.min()) },{ int2(split3.first.max()) } }; // `the rest` split by the same amount
    ui_rect bottomRightPane = { { int2(split3.second.min()) },{ int2(split3.second.max()) } }; // remainder

    gui::imgui_fixed_window_begin("Inspector", topRightPane);
    if (editor->get_selection().size() >= 1)
    {
        InspectGameObjectPolymorphic(nullptr, editor->get_selection()[0]);
    }
    gui::imgui_fixed_window_end();

    gui::imgui_fixed_window_begin("Materials", middleRightPane);
    std::vector<std::string> mats;
    for (auto & m : AssetHandle<std::shared_ptr<Material>>::list()) mats.push_back(m.name);
    static int selectedMaterial = 1;
    ImGui::ListBox("Material", &selectedMaterial, mats);
    if (mats.size() >= 1)
    {
        auto w = AssetHandle<std::shared_ptr<Material>>::list()[selectedMaterial].get();
        InspectGameObjectPolymorphic(nullptr, w.get());
    }
    gui::imgui_fixed_window_end();

    // Scene Object List
    gui::imgui_fixed_window_begin("Objects", bottomRightPane);

    for (size_t i = 0; i < scene.objects.size(); ++i)
    {
        ImGui::PushID(static_cast<int>(i));
        bool selected = editor->selected(scene.objects[i].get());
        std::string name = scene.objects[i]->id.size() > 0 ? scene.objects[i]->id : std::string(typeid(*scene.objects[i]).name()); // For polymorphic typeids, the trick is to dereference it first
        std::vector<StaticMesh *> selectedObjects;

        if (ImGui::Selectable(name.c_str(), &selected))
        {
            if (!ImGui::GetIO().KeyCtrl) editor->clear();
            editor->update_selection(scene.objects[i].get());
        }
        ImGui::PopID();
    }
    gui::imgui_fixed_window_end();

    // Renderer
    gui::imgui_fixed_window_begin("Renderer Settings", { { 0, 17 }, { 320, height } });
    ImGui::Text("Compute %f", computeTimer.elapsed_ms());
    ImGui::Checkbox("Ambient Occlusion", &enableAo);
    renderer->gather_imgui();
    gui::imgui_fixed_window_end();

    igm->end_frame();

    // Scene editor gizmo
    glClear(GL_DEPTH_BUFFER_BIT);
    editor->on_draw();

    gl_check_error(__FILE__, __LINE__);

    glfwSwapBuffers(window); 
}

IMPLEMENT_MAIN(int argc, char * argv[])
{
    try
    {
        scene_editor_app app;
        app.main_loop();
    }
    catch (const std::exception & e)
    {
        std::cerr << "Application Fatal: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
