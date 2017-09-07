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

/*
    void Dispatch( size_t GroupCountX = 1, size_t GroupCountY = 1, size_t GroupCountZ = 1 );
    void Dispatch1D( size_t ThreadCountX, size_t GroupSizeX = 64);
    void Dispatch2D( size_t ThreadCountX, size_t ThreadCountY, size_t GroupSizeX = 8, size_t GroupSizeY = 8);
    void Dispatch3D( size_t ThreadCountX, size_t ThreadCountY, size_t ThreadCountZ, size_t GroupSizeX, size_t GroupSizeY, size_t GroupSizeZ );
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

    GlComputeProgram linearizeDepth = preprocess_compute_defines(read_file_text("../assets/shaders/ssao/linearize_depth_comp.glsl"), {});

    GlComputeProgram prepareDepth1 = preprocess_compute_defines(read_file_text("../assets/shaders/ssao/ao_prepare_depth1_comp.glsl"), {});
    GlComputeProgram prepareDepth2 = preprocess_compute_defines(read_file_text("../assets/shaders/ssao/ao_prepare_depth2_comp.glsl"), {});

    GlComputeProgram aoRender1 = preprocess_compute_defines(read_file_text("../assets/shaders/ssao/ao_render.glsl"), { "INTERLEAVE_RESULT" });
    GlComputeProgram aoRender2 = preprocess_compute_defines(read_file_text("../assets/shaders/ssao/ao_render.glsl"), {});

    GlComputeProgram blendOut = preprocess_compute_defines(read_file_text("../assets/shaders/ssao/ao_blur_and_upsample_comp.glsl"), { "BLEND_WITH_HIGHER_RESOLUTION" });
    GlComputeProgram preMinBlendOut = preprocess_compute_defines(read_file_text("../assets/shaders/ssao/ao_blur_and_upsample_comp.glsl"), { "COMBINE_LOWER_RESOLUTIONS", "BLEND_WITH_HIGHER_RESOLUTION" });
    GlComputeProgram upsample = preprocess_compute_defines(read_file_text("../assets/shaders/ssao/ao_blur_and_upsample_comp.glsl"), {});
    GlComputeProgram PreMin = preprocess_compute_defines(read_file_text("../assets/shaders/ssao/ao_blur_and_upsample_comp.glsl"), { "COMBINE_LOWER_RESOLUTIONS" });

    float SampleThickness[12]; // Pre-computed sample thicknesses

    ScreenSpaceAmbientOcclusionPass()
    {

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

    void compute(GlTexture2D & Destination, GlTexture2D & DepthBuffer, const float TanHalfFovH)
    {
        size_t BufferWidth = DepthBuffer.width;
        size_t BufferHeight = DepthBuffer.height;

        size_t ArrayCount = DepthBuffer.GetDepth();

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

        __declspec(align(16)) float SsaoCB[28];

        // The thicknesses are smaller for all off-center samples of the sphere.  Compute thicknesses relative
        // to the center sample.
        SsaoCB[0] = InverseRangeFactor / SampleThickness[0];
        SsaoCB[1] = InverseRangeFactor / SampleThickness[1];
        SsaoCB[2] = InverseRangeFactor / SampleThickness[2];
        SsaoCB[3] = InverseRangeFactor / SampleThickness[3];
        SsaoCB[4] = InverseRangeFactor / SampleThickness[4];
        SsaoCB[5] = InverseRangeFactor / SampleThickness[5];
        SsaoCB[6] = InverseRangeFactor / SampleThickness[6];
        SsaoCB[7] = InverseRangeFactor / SampleThickness[7];
        SsaoCB[8] = InverseRangeFactor / SampleThickness[8];
        SsaoCB[9] = InverseRangeFactor / SampleThickness[9];
        SsaoCB[10] = InverseRangeFactor / SampleThickness[10];
        SsaoCB[11] = InverseRangeFactor / SampleThickness[11];

        // These are the weights that are multiplied against the samples because not all samples are
        // equally important.  The farther the sample is from the center location, the less they matter.
        // We use the thickness of the sphere to determine the weight.  The scalars in front are the number
        // of samples with this weight because we sum the samples together before multiplying by the weight,
        // so as an aggregate all of those samples matter more.  After generating this table, the weights
        // are normalized.
        SsaoCB[12] = 4.0f * SampleThickness[0];	 // Axial
        SsaoCB[13] = 4.0f * SampleThickness[1];	 // Axial
        SsaoCB[14] = 4.0f * SampleThickness[2];	 // Axial
        SsaoCB[15] = 4.0f * SampleThickness[3];	 // Axial
        SsaoCB[16] = 4.0f * SampleThickness[4];	 // Diagonal
        SsaoCB[17] = 8.0f * SampleThickness[5];	 // L-shaped
        SsaoCB[18] = 8.0f * SampleThickness[6];	 // L-shaped
        SsaoCB[19] = 8.0f * SampleThickness[7];	 // L-shaped
        SsaoCB[20] = 4.0f * SampleThickness[8];	 // Diagonal
        SsaoCB[21] = 8.0f * SampleThickness[9];	 // L-shaped
        SsaoCB[22] = 8.0f * SampleThickness[10]; // L-shaped
        SsaoCB[23] = 4.0f * SampleThickness[11]; // Diagonal

        // Normalize the weights by dividing by the sum of all weights
        float totalWeight = 0.0f;
        for (int i = 12; i < 24; ++i) totalWeight += SsaoCB[i];
        for (int i = 12; i < 24; ++i) SsaoCB[i] /= totalWeight;

        SsaoCB[24] = 1.0f / BufferWidth;
        SsaoCB[25] = 1.0f / BufferHeight;
        SsaoCB[26] = 1.0f / -RejectionFalloff;
        SsaoCB[27] = 1.0f / (1.0f + Accentuation);

        // Main_interleaved
        cmd.SetComputeFloatParams(cs, "gInvThicknessTable", InvThicknessTable);
        cmd.SetComputeFloatParams(cs, "gSampleWeightTable", SampleWeightTable);
        cmd.SetComputeVectorParam(cs, "gInvSliceDimension", source.inverseDimensions);
        cmd.SetComputeFloatParam(cs, "gRejectFadeoff", -1 / _thicknessModifier);
        cmd.SetComputeFloatParam(cs, "gIntensity", _intensity);

        cmd.SetComputeTextureParam(cs, kernel, "DepthTex", source.id);
        cmd.SetComputeTextureParam(cs, kernel, "Occlusion", dest.id);

        // Calculate the thread group count and add a dispatch command with them.

        uint32_t xsize, ysize, zsize;
        cs.GetKernelThreadGroupSizes(kernel, out xsize, out ysize, out zsize);

        cmd.DispatchCompute(
            cs, kernel,
            (source.width + (int)xsize - 1) / (int)xsize,
            (source.height + (int)ysize - 1) / (int)ysize,
            (source.depth + (int)zsize - 1) / (int)zsize
        );

    }

};

scene_editor_app::scene_editor_app() : GLFWApp(1920, 1080, "Scene Editor")
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    igm.reset(new gui::ImGuiManager(window));
    gui::make_dark_theme();

    // Linearization - LinearizeDepth

    //GlComputeProgram p(read_file_text("../assets/shaders/ao_prepare_depth2_comp.glsl"));
    //std::cout << "Max Threads Per Group: " << p.get_max_threads_per_workgroup() << std::endl;
    //std::cout << "Max Workgroup Size:    " << p.get_max_workgroup_size() << std::endl;

    editor.reset(new editor_controller<GameObject>());

    ScreenSpaceAmbientOcclusionPass ssao;

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

    // Register all material instances with the asset system. Since everything is handle-based,
    // we can do this wherever, so long as it's before the first rendered frame
    for (auto & instance : scene.materialInstances)
    {
        std::cout << "Registering: " << instance.first.c_str() << std::endl;
        global_register_asset(instance.first.c_str(), static_cast<std::shared_ptr<Material>>(instance.second));
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
