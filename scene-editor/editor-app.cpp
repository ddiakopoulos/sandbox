#include "index.hpp"
#include "editor-app.hpp"
#include "gui.hpp"
#include "serialization.hpp"

using namespace avl;

// Linearize
//  * writeonly LinearZ
//  * uniform sampler2D Depth
//  * CB0 with ZMagic

// Downsample Passes
//  * 
//  *

// Blur and Upsample Passes
//  * 
//  * 
//  * 
//  * 

// AO Render Passes
//  *
//  *
//

/*
    void Dispatch( size_t GroupCountX = 1, size_t GroupCountY = 1, size_t GroupCountZ = 1 );
    void Dispatch1D( size_t ThreadCountX, size_t GroupSizeX = 64);
    void Dispatch2D( size_t ThreadCountX, size_t ThreadCountY, size_t GroupSizeX = 8, size_t GroupSizeY = 8);
    void Dispatch3D( size_t ThreadCountX, size_t ThreadCountY, size_t ThreadCountZ, size_t GroupSizeX, size_t GroupSizeY, size_t GroupSizeZ );

    Fixed, Half, Float,                        // 2D render texture
    FixedUAV, HalfUAV, FloatUAV,               // Read/write enabled
    FixedTiledUAV, HalfTiledUAV, FloatTiledUAV // Texture array

    //GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format
*/

struct ScreenSpaceAmbientOcclusionPass
{
    // Controls how aggressive to fade off samples that occlude spheres but by so much as to be unreliable.
    // This is what gives objects a dark halo around them when placed in front of a wall.  If you want to
    // fade off the halo, boost your rejection falloff.  The tradeoff is that it reduces overall AO.
    float RejectionFalloff = 2.5f;  // 1.0f, 10.0f, 0.5f;

    // The effect normally marks anything that's 50% occluded or less as "fully unoccluded".  This throws away
    // half of our result.  Accentuation gives more range to the effect, but it will darken all AO values in the
    // process.  It will also cause "under occluded" geometry to appear to be highlighted.  If your ambient light
    // is determined by the surface normal (such as with IBL), you might not want this side effect.
    float Accentuation = 0.1f; // 0.0f, 1.0f, 0.1f;

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

    GlComputeProgram linearizeDepth = preprocess_compute_defines(read_file_text("../assets/shaders/ssao/linearize_depth_comp.glsl"), {});

    GlComputeProgram prepareDepth1 = preprocess_compute_defines(read_file_text("../assets/shaders/ssao/ao_prepare_depth1_comp.glsl"), {});
    GlComputeProgram prepareDepth2 = preprocess_compute_defines(read_file_text("../assets/shaders/ssao/ao_prepare_depth2_comp.glsl"), {});

    GlComputeProgram aoRender1 = preprocess_compute_defines(read_file_text("../assets/shaders/ssao/ao_render.glsl"), { "INTERLEAVE_RESULT" });
    GlComputeProgram aoRender2 = preprocess_compute_defines(read_file_text("../assets/shaders/ssao/ao_render.glsl"), {});

    GlComputeProgram blendOut = preprocess_compute_defines(read_file_text("../assets/shaders/ssao/ao_blur_and_upsample_comp.glsl"), { "BLEND_WITH_HIGHER_RESOLUTION" });
    GlComputeProgram preMinBlendOut = preprocess_compute_defines(read_file_text("../assets/shaders/ssao/ao_blur_and_upsample_comp.glsl"), { "COMBINE_LOWER_RESOLUTIONS", "BLEND_WITH_HIGHER_RESOLUTION" });
    GlComputeProgram upsample = preprocess_compute_defines(read_file_text("../assets/shaders/ssao/ao_blur_and_upsample_comp.glsl"), {});
    GlComputeProgram PreMin = preprocess_compute_defines(read_file_text("../assets/shaders/ssao/ao_blur_and_upsample_comp.glsl"), { "COMBINE_LOWER_RESOLUTIONS" });

    std::vector<float> SampleThickness; // Pre-computed sample thicknesses
    std::vector<float> InvThicknessTable;
    std::vector<float> SampleWeightTable;

    // Calculate values in _ZBuferParams (built-in shader variable)
    // We can't use _ZBufferParams in compute shaders, so this function is
    // used to give the values in it to compute shaders.
    float4 CalculateZBufferParams(float nearClip, float farClip)
    {
        float fpn = farClip / nearClip;
        return float4(1 - fpn, fpn, 0, 0);
    }

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

    ScreenSpaceAmbientOcclusionPass()
    {
        const float2 renderSize = { 1920, 1080 };

        SampleThickness.resize(12);
        InvThicknessTable.resize(12);
        SampleWeightTable.resize(12);

        depthLinear.setup(renderSize.x, renderSize.y, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        ambientOcclusion.setup(renderSize.x, renderSize.y, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

        {
            int2 ldSize1 = CalculateMipSize(float2(renderSize.x, renderSize.y), 1);
            int2 ldSize2 = CalculateMipSize(float2(renderSize.x, renderSize.y), 2);
            int2 ldSize3 = CalculateMipSize(float2(renderSize.x, renderSize.y), 3);
            int2 ldSize4 = CalculateMipSize(float2(renderSize.x, renderSize.y), 4);
            lowDepth1.setup(ldSize1.x, ldSize1.y, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            lowDepth2.setup(ldSize2.x, ldSize2.y, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            lowDepth3.setup(ldSize3.x, ldSize3.y, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            lowDepth4.setup(ldSize4.x, ldSize4.y, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        }

        gl_check_error(__FILE__, __LINE__);

        {
            int2 combSize1 = CalculateMipSize(float2(renderSize.x, renderSize.y), 1);
            int2 combSize2 = CalculateMipSize(float2(renderSize.x, renderSize.y), 2);
            int2 combSize3 = CalculateMipSize(float2(renderSize.x, renderSize.y), 3);
            combined1.setup(combSize1.x, combSize1.y, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            combined2.setup(combSize2.x, combSize2.y, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            combined3.setup(combSize3.x, combSize3.y, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        }

        gl_check_error(__FILE__, __LINE__);

        {
            int2 tdSize1 = CalculateMipSize(float2(renderSize.x, renderSize.y), 3);
            int2 tdSize2 = CalculateMipSize(float2(renderSize.x, renderSize.y), 4);
            int2 tdSize3 = CalculateMipSize(float2(renderSize.x, renderSize.y), 5);
            int2 tdSize4 = CalculateMipSize(float2(renderSize.x, renderSize.y), 6);
            tiledDepth1.setup(GL_TEXTURE_2D_ARRAY, tdSize1.x, tdSize1.y, 4, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr); // 3
            tiledDepth2.setup(GL_TEXTURE_2D_ARRAY, tdSize2.x, tdSize2.y, 4, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr); // 4
            tiledDepth3.setup(GL_TEXTURE_2D_ARRAY, tdSize3.x, tdSize3.y, 4, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr); // 5
            tiledDepth4.setup(GL_TEXTURE_2D_ARRAY, tdSize4.x, tdSize4.y, 4, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr); // 6
        }

        gl_check_error(__FILE__, __LINE__);

        {
            int2 ocSize1 = CalculateMipSize(float2(renderSize.x, renderSize.y), 1);
            int2 ocSize2 = CalculateMipSize(float2(renderSize.x, renderSize.y), 2);
            int2 ocSize3 = CalculateMipSize(float2(renderSize.x, renderSize.y), 3);
            int2 ocSize4 = CalculateMipSize(float2(renderSize.x, renderSize.y), 4);
            occlusion1.setup(ocSize1.x, ocSize1.y, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            occlusion2.setup(ocSize2.x, ocSize2.y, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            occlusion3.setup(ocSize3.x, ocSize3.y, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            occlusion4.setup(ocSize4.x, ocSize4.y, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        }

        gl_check_error(__FILE__, __LINE__);

        SampleThickness[0] = sqrt(1.0f - 0.2f * 0.2f);
        SampleThickness[1] = sqrt(1.0f - 0.4f * 0.4f);
        SampleThickness[2] = sqrt(1.0f - 0.6f * 0.6f);
        SampleThickness[3] = sqrt(1.0f - 0.8f * 0.8f);
        SampleThickness[4] = sqrt(1.0f - 0.2f * 0.2f - 0.2f * 0.2f);
        SampleThickness[5] = sqrt(1.0f - 0.2f * 0.2f - 0.4f * 0.4f);
        SampleThickness[6] = sqrt(1.0f - 0.2f * 0.2f - 0.6f * 0.6f);
        SampleThickness[7] = sqrt(1.0f - 0.2f * 0.2f - 0.8f * 0.8f);
        SampleThickness[8] = sqrt(1.0f - 0.4f * 0.4f - 0.4f * 0.4f);
        SampleThickness[9] = sqrt(1.0f - 0.4f * 0.4f - 0.6f * 0.6f);
        SampleThickness[10] = sqrt(1.0f - 0.4f * 0.4f - 0.8f * 0.8f);
        SampleThickness[11] = sqrt(1.0f - 0.6f * 0.6f - 0.6f * 0.6f);
    } 
    
    void PushDownsampleCommands(const GLuint depthSource)
    {
        // First downsampling pass.
        {
            glUseProgram(prepareDepth1.handle());
            glBindImageTexture(0, depthLinear, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
            gl_check_error(__FILE__, __LINE__);
            glBindImageTexture(1, lowDepth1, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
            gl_check_error(__FILE__, __LINE__);
            glBindImageTexture(2, tiledDepth1, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_R32F);
            gl_check_error(__FILE__, __LINE__);
            glBindImageTexture(3, lowDepth2, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
            gl_check_error(__FILE__, __LINE__);
            glBindImageTexture(4, tiledDepth2, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_R32F);
            gl_check_error(__FILE__, __LINE__);
            prepareDepth1.uniform("ZBufferParams", CalculateZBufferParams(0.1, 24.f)); // FIXME
            gl_check_error(__FILE__, __LINE__);
            prepareDepth1.texture("Depth", 0, depthSource, GL_TEXTURE_2D); // sampler 
            gl_check_error(__FILE__, __LINE__);
            prepareDepth1.dispatch(tiledDepth2.width, tiledDepth2.height, 1);
            gl_check_error(__FILE__, __LINE__);
        }

        gl_check_error(__FILE__, __LINE__);

        // Second downsampling pass.
        {
            glUseProgram(prepareDepth2.handle());
            glBindImageTexture(0, lowDepth3, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
            glBindImageTexture(1, tiledDepth3, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_R32F);
            glBindImageTexture(2, lowDepth4, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
            glBindImageTexture(3, tiledDepth4, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_R32F);
            prepareDepth2.texture("DX4x", 0, lowDepth2, GL_TEXTURE_2D); // sampler 
            prepareDepth2.dispatch(tiledDepth4.width, tiledDepth4.height, 1);
        }

        gl_check_error(__FILE__, __LINE__);
    }

    void PushRenderAOCommands(const GLuint source, const GLuint dest, const float TanHalfFovH, const size_t BufferWidth, const size_t BufferHeight, const size_t ArrayCount)
    {
        // Here we compute multipliers that convert the center depth value into (the reciprocal of)
        // sphere thicknesses at each sample location.  This assumes a maximum sample radius of 5
        // units, but since a sphere has no thickness at its extent, we don't need to sample that far
        // out.  Only samples whole integer offsets with distance less than 25 are used.  This means
        // that there is no sample at (3, 4) because its distance is exactly 25 (and has a thickness of 0.)

        // The shaders are set up to sample a circular region within a 5-pixel radius.
        const float ScreenspaceDiameter = 10.0f;

        // SphereDiameter = CenterDepth * ThicknessMultiplier.  This will compute the thickness of a sphere centered
        // at a specific depth.  The ellipsoid scale can stretch a sphere into an ellipsoid, which changes the
        // characteristics of the AO.
        // TanHalfFovH:  Radius of sphere in depth units if its center lies at Z = 1
        // ScreenspaceDiameter:  Diameter of sample sphere in pixel units
        // ScreenspaceDiameter / BufferWidth:  Ratio of the screen width that the sphere actually covers
        // Note about the "2.0f * ":  Diameter = 2 * Radius
        float ThicknessMultiplier = 2.0f * TanHalfFovH * ScreenspaceDiameter / BufferWidth;

        if (ArrayCount == 1) ThicknessMultiplier *= 2.0f;

        // This will transform a depth value from [0, thickness] to [0, 1].
        float InverseRangeFactor = 1.0f / ThicknessMultiplier;

        // The thicknesses are smaller for all off-center samples of the sphere.  Compute thicknesses relative
        // to the center sample.
        for (int i = 0; i < 12; i++) InvThicknessTable[i] = InverseRangeFactor / SampleThickness[i];

        // These are the weights that are multiplied against the samples because not all samples are
        // equally important.  The farther the sample is from the center location, the less they matter.
        // We use the thickness of the sphere to determine the weight.  The scalars in front are the number
        // of samples with this weight because we sum the samples together before multiplying by the weight,
        // so as an aggregate all of those samples matter more.  After generating this table, the weights
        // are normalized.
        SampleWeightTable[0] = 4 * SampleThickness[0];    // Axial
        SampleWeightTable[1] = 4 * SampleThickness[1];    // Axial
        SampleWeightTable[2] = 4 * SampleThickness[2];    // Axial
        SampleWeightTable[3] = 4 * SampleThickness[3];    // Axial
        SampleWeightTable[4] = 4 * SampleThickness[4];    // Diagonal
        SampleWeightTable[5] = 8 * SampleThickness[5];    // L-shaped
        SampleWeightTable[6] = 8 * SampleThickness[6];    // L-shaped
        SampleWeightTable[7] = 8 * SampleThickness[7];    // L-shaped
        SampleWeightTable[8] = 4 * SampleThickness[8];    // Diagonal
        SampleWeightTable[9] = 8 * SampleThickness[9];    // L-shaped
        SampleWeightTable[10] = 8 * SampleThickness[10];  // L-shaped
        SampleWeightTable[11] = 4 * SampleThickness[11];  // Diagonal

        // Normalize the weights by dividing by the sum of all weights
        float totalWeight = 0.0f;
        for(auto & w : SampleWeightTable) totalWeight += w;

        for (int i = 0; i < 12; ++i) SampleWeightTable[i] /= totalWeight;

        glUseProgram(aoRender1.handle());
        aoRender1.uniform("gInvThicknessTable", InvThicknessTable);
        aoRender1.uniform("gSampleWeightTable", SampleWeightTable);
        aoRender1.uniform("gInvSliceDimension", float2(1.0f / BufferWidth, 1.0f / BufferHeight));
        aoRender1.uniform("gRejectFadeoff", -1.f / 1.0f);
        aoRender1.uniform("gIntensity", 1.0f);

        gl_check_error(__FILE__, __LINE__);

        //https://stackoverflow.com/questions/37136813/what-is-the-difference-between-glbindimagetexture-and-glbindtexture

        aoRender1.texture("DepthTex", 0, source, GL_TEXTURE_2D_ARRAY); // array vs not

        gl_check_error(__FILE__, __LINE__);

        //glBindImageTexture(0, source, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32F); //cmd.SetComputeTextureParam(cs, kernel, "DepthTex", source.id);
        glBindImageTexture(0, dest, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F); //cmd.SetComputeTextureParam(cs, kernel, "Occlusion", dest.id);

        gl_check_error(__FILE__, __LINE__);

        // Calculate the thread group count and add a dispatch command with them.
        auto workgroupSize = aoRender1.get_max_workgroup_size();

        workgroupSize.x = (BufferWidth +  (int) workgroupSize.x - 1) / (int) workgroupSize.x;
        workgroupSize.y = (BufferHeight + (int) workgroupSize.y - 1) / (int) workgroupSize.y;
        workgroupSize.z = (ArrayCount +   (int) workgroupSize.z - 1) / (int) workgroupSize.z;

        aoRender1.dispatch(workgroupSize);

        gl_check_error(__FILE__, __LINE__);
    }

    void PushUpsampleCommands(const GlTexture2D & lowResDepth, const GlTexture2D & interleavedAO, const GlTexture2D & highResDepth, const GlTexture2D & highResAO, const GlTexture2D & dest, bool highRes)
    {
        GlComputeProgram & prog = (highRes == false) ? upsample : blendOut;

        const float stepSize = 1920.0f / lowResDepth.width; // fix hardcoded resolution
        float blurTolerance = (1.0 - std::pow(10, -4.6f) * stepSize);
        blurTolerance *= blurTolerance;
        const float upsampleTolerance = std::pow(10, -12.f);
        const float noiseFilterWeight = 1 / (std::pow(10, 0.0f) + upsampleTolerance); // noise filter tolerance

        prog.uniform("InvLowResolution", float2( 1.f / lowResDepth.width, 1.f / lowResDepth.height));
        prog.uniform("InvHighResolution", float2(1.f / highResDepth.width, 1.f / highResDepth.height));
        prog.uniform("NoiseFilterStrength", noiseFilterWeight);
        prog.uniform("StepSize", stepSize);
        prog.uniform("kBlurTolerance", blurTolerance);
        prog.uniform("kUpsampleTolerance", upsampleTolerance);

        prog.texture("LoResDB", 0, lowResDepth, GL_TEXTURE_2D);
        prog.texture("HiResDB", 1, highResDepth, GL_TEXTURE_2D);
        prog.texture("LoResAO1", 2, interleavedAO, GL_TEXTURE_2D);

        if (highRes != false) prog.texture("HiResAO", 3, highResAO, GL_TEXTURE_2D);

        glBindImageTexture(0, dest, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F); // AoResult

        int xcount = (highResDepth.width + 17) / 16;
        int ycount = (highResDepth.height + 17) / 16;
        prog.dispatch(xcount, ycount, 1);
    }

    void RebuildCommandBuffers(const GLuint depthSrc)
    {
        PushDownsampleCommands(depthSrc);

        float tanHalfFovH = CalculateTanHalfFovHeight(Identity4x4); // FIXME FIXME
        PushRenderAOCommands(tiledDepth1, occlusion1, tanHalfFovH, tiledDepth1.width, tiledDepth1.height, tiledDepth1.depth); // src, dest
        PushRenderAOCommands(tiledDepth2, occlusion2, tanHalfFovH, tiledDepth2.width, tiledDepth2.height, tiledDepth1.depth);
        PushRenderAOCommands(tiledDepth3, occlusion3, tanHalfFovH, tiledDepth3.width, tiledDepth3.height, tiledDepth1.depth);
        PushRenderAOCommands(tiledDepth4, occlusion4, tanHalfFovH, tiledDepth4.width, tiledDepth4.height, tiledDepth1.depth);

        PushUpsampleCommands(lowDepth4, occlusion4, lowDepth3, occlusion3, combined3, false);
        PushUpsampleCommands(lowDepth3, combined3, lowDepth2, occlusion2, combined2, false);
        PushUpsampleCommands(lowDepth2, combined2, lowDepth1, occlusion1, combined1, false);
        PushUpsampleCommands(lowDepth1, combined1, depthLinear, occlusion1, ambientOcclusion, true); // occlusion1 again is fake
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
    //auto shaderball = load_geometry_from_ply("../assets/models/geometry/CubeHollowOpen.ply");
    //rescale_geometry(shaderball, 1.f);
    //global_register_asset("shaderball", make_mesh_from_geometry(shaderball));
    //global_register_asset("shaderball", std::move(shaderball));

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
            m.mesh = GlMeshHandle("icosphere");
            m.geom = GeometryHandle("icosphere");
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
    floorMesh.set_pose(Pose(float3(0, -2.01f, 0)));
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

        // SSAO Testing
        {
            computeTimer.start();
            ssao->RebuildCommandBuffers(renderer->get_output_texture(0));
            computeTimer.stop();
        }

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
