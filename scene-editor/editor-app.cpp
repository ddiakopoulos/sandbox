#include "index.hpp"
#include "editor-app.hpp"
#include "gui.hpp"
#include "serialization.hpp"
#include "logging.hpp"

using namespace avl;

scene_editor_app::scene_editor_app() : GLFWApp(1920, 1080, "Scene Editor")
{
    std::cout << "Timestamp: " << HumanTime().make_timestamp() << std::endl;

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    igm.reset(new gui::ImGuiInstance(window));
    gui::make_light_theme();

    editor.reset(new editor_controller<GameObject>());

    cam.look_at({ 0, 9.5f, -6.0f }, { 0, 0.1f, 0 });
    flycam.set_camera(&cam);

    Logger::get_instance()->add_sink(std::make_shared<ImGui::LogWindowSink>(log));

    auto wireframeProgram = GlShader(
        read_file_text("../assets/shaders/wireframe_vert.glsl"), 
        read_file_text("../assets/shaders/wireframe_frag.glsl"),
        read_file_text("../assets/shaders/wireframe_geom.glsl"));
    create_handle_for_asset("wireframe", std::move(wireframeProgram));

    shaderMonitor.watch(
        "../assets/shaders/renderer/depth_prepass_vert.glsl",
        "../assets/shaders/renderer/depth_prepass_frag.glsl",
        "../assets/shaders/renderer", {}, [](GlShader shader)
    {
        create_handle_for_asset("depth-prepass", std::move(shader));
    });

    shaderMonitor.watch(
        "../assets/shaders/renderer/forward_lighting_vert.glsl",
        "../assets/shaders/renderer/default_material_frag.glsl",
        "../assets/shaders/renderer", {}, [](GlShader shader)
    {
        create_handle_for_asset("default-shader", std::move(shader));
    });

    pbrProgramAsset = shaderMonitor.watch(
        "../assets/shaders/renderer/forward_lighting_vert.glsl", 
        "../assets/shaders/renderer/forward_lighting_frag.glsl", 
        "../assets/shaders/renderer", 
        {"TWO_CASCADES", "USE_PCF_3X3", "ENABLE_SHADOWS", 
         "USE_IMAGE_BASED_LIGHTING", 
         "HAS_ROUGHNESS_MAP", "HAS_METALNESS_MAP", "HAS_ALBEDO_MAP", "HAS_NORMAL_MAP", "HAS_OCCLUSION_MAP"}, [](GlShader shader)
    {
        create_handle_for_asset("pbr-forward-lighting", std::move(shader));
    });

    shaderMonitor.watch(
        "../assets/shaders/renderer/shadowcascade_vert.glsl", 
        "../assets/shaders/renderer/shadowcascade_frag.glsl", 
        "../assets/shaders/renderer/shadowcascade_geom.glsl", 
        "../assets/shaders/renderer", {}, 
        [](GlShader shader) 
    {
        create_handle_for_asset("cascaded-shadows", std::move(shader));
    });

    shaderMonitor.watch(
        "../assets/shaders/renderer/post_tonemap_vert.glsl",
        "../assets/shaders/renderer/post_tonemap_frag.glsl",
        [](GlShader shader) 
    {
        create_handle_for_asset("post-tonemap", std::move(shader));
    });

    RendererSettings settings;
    settings.renderSize = float2(width, height);
    renderer.reset(new PhysicallyBasedRenderer(settings));

    scene.skybox.reset(new HosekProceduralSky());
    renderer->set_procedural_sky(scene.skybox.get());
    scene.skybox->onParametersChanged = [&]
    {
        uniforms::directional_light updatedSun;
        updatedSun.direction = scene.skybox->get_sun_direction();
        updatedSun.color = float3(1.f, 1.0f, 1.0f);
        updatedSun.amount = 1.0f;
        renderer->set_sunlight(updatedSun);
    };

    auto radianceBinary = read_file_binary("../assets/textures/envmaps/wells_radiance.dds");
    auto irradianceBinary = read_file_binary("../assets/textures/envmaps/wells_irradiance.dds");
    gli::texture_cube radianceHandle(gli::load_dds((char *)radianceBinary.data(), radianceBinary.size()));
    gli::texture_cube irradianceHandle(gli::load_dds((char *)irradianceBinary.data(), irradianceBinary.size()));
    create_handle_for_asset("wells-radiance-cubemap", load_cubemap(radianceHandle));
    create_handle_for_asset("wells-irradiance-cubemap", load_cubemap(irradianceHandle));

    create_handle_for_asset("rusted-iron-albedo", load_image("../assets/nonfree/Metal_ModernMetalIsoDiamondTile_2k_basecolor.tga", false));
    create_handle_for_asset("rusted-iron-normal", load_image("../assets/nonfree/Metal_ModernMetalIsoDiamondTile_2k_n.tga", false));
    create_handle_for_asset("rusted-iron-metallic", load_image("../assets/nonfree/Metal_ModernMetalIsoDiamondTile_2k_metallic.tga", false));
    create_handle_for_asset("rusted-iron-roughness", load_image("../assets/nonfree/Metal_ModernMetalIsoDiamondTile_2k_roughness.tga", false));
    create_handle_for_asset("rusted-iron-occlusion", load_image("../assets/nonfree/Metal_ModernMetalIsoDiamondTile_2k_ao.tga", false));

    create_handle_for_asset("scifi-floor-albedo", load_image("../assets/nonfree/Metal_ScifiHangarFloor_2k_basecolor.tga", false));
    create_handle_for_asset("scifi-floor-normal", load_image("../assets/nonfree/Metal_ScifiHangarFloor_2k_n.tga", false));
    create_handle_for_asset("scifi-floor-metallic", load_image("../assets/nonfree/Metal_ScifiHangarFloor_2k_metallic.tga", false));
    create_handle_for_asset("scifi-floor-roughness", load_image("../assets/nonfree/Metal_ScifiHangarFloor_2k_roughness.tga", false));
    create_handle_for_asset("scifi-floor-occlusion", load_image("../assets/nonfree/Metal_ScifiHangarFloor_2k_ao.tga", false));

    std::shared_ptr<DefaultMaterial> default = std::make_shared<DefaultMaterial>();
    create_handle_for_asset("default-material", static_cast<std::shared_ptr<Material>>(default));

    cereal::deserialize_from_json("../assets/materials.json", scene.materialInstances);

    // Register all material instances with the asset system. Since everything is handle-based,
    // we can do this wherever, so long as it's before the first rendered frame
    for (auto & instance : scene.materialInstances)
    {
        create_handle_for_asset(instance.first.c_str(), static_cast<std::shared_ptr<Material>>(instance.second));
    }

    //auto shaderball = load_geometry_from_ply("../assets/models/shaderball/shaderball.ply");
    auto shaderball = load_geometry_from_ply("../assets/models/geometry/TorusKnotUniform.ply");
    rescale_geometry(shaderball, 1.f);
    create_handle_for_asset("shaderball", make_mesh_from_geometry(shaderball));
    create_handle_for_asset("shaderball", std::move(shaderball));

    auto ico = make_icosasphere(5);
    create_handle_for_asset("icosphere", make_mesh_from_geometry(ico));
    create_handle_for_asset("icosphere", std::move(ico));

    auto cube = make_cube();
    create_handle_for_asset("cube", make_mesh_from_geometry(cube));
    create_handle_for_asset("cube", std::move(cube));

    scene.objects.clear();
    cereal::deserialize_from_json("../assets/scene.json", scene.objects);

    // Setup Debug visualizations
    uiSurface.bounds = { 0, 0, (float)width, (float)height };
    uiSurface.add_child({ { 0.0000f, +20 },{ 0, +20 },{ 0.1667f, -10 },{ 0.133f, +10 } });
    uiSurface.add_child({ { 0.1667f, +20 },{ 0, +20 },{ 0.3334f, -10 },{ 0.133f, +10 } });
    uiSurface.add_child({ { 0.3334f, +20 },{ 0, +20 },{ 0.5009f, -10 },{ 0.133f, +10 } });
    uiSurface.add_child({ { 0.5000f, +20 },{ 0, +20 },{ 0.6668f, -10 },{ 0.133f, +10 } });
    uiSurface.layout();

    debugViews.push_back(std::make_shared<GLTextureView>(true));
    debugViews.push_back(std::make_shared<GLTextureView>(true, float2(cam.nearclip, cam.farclip)));
}

scene_editor_app::~scene_editor_app()
{ 

}

void scene_editor_app::on_drop(std::vector<std::string> filepaths)
{
    for (auto path : filepaths)
    {
        std::transform(path.begin(), path.end(), path.begin(), ::tolower);
        const std::string fileExtension = get_extension(path);

        if (fileExtension == "png" || fileExtension == "tga" || fileExtension == "jpg")
        {
            create_handle_for_asset(get_filename_without_extension(path).c_str(), load_image(path, false));
        }

        if (fileExtension == "ply")
        {
            auto plyImport = load_geometry_from_ply(path);
            rescale_geometry(plyImport, 1.f);
            create_handle_for_asset(get_filename_without_extension(path).c_str(), make_mesh_from_geometry(plyImport));
            create_handle_for_asset(get_filename_without_extension(path).c_str(), std::move(plyImport));
        }

        if (fileExtension == "obj")
        {
            auto objImport = load_geometry_from_obj_no_texture(path);
            for (auto & mesh : objImport)
            {
                rescale_geometry(mesh, 1.f);
                create_handle_for_asset(get_filename_without_extension(path).c_str(), make_mesh_from_geometry(mesh));
                create_handle_for_asset(get_filename_without_extension(path).c_str(), std::move(mesh));
            }
        }
    }
}

void scene_editor_app::on_window_resize(int2 size) 
{ 
    uiSurface.bounds = { 0, 0, (float)size.x, (float)size.y };
    uiSurface.layout();
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

    const Pose cameraPose = cam.get_pose();
    const float4x4 projectionMatrix = cam.get_projection_matrix(float(width) / float(height));
    const float4x4 viewMatrix = cam.get_view_matrix();
    const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);

    {
        // Single-viewport camera
        renderer->add_camera(0, cameraPose, projectionMatrix);

        // Lighting
        for (auto & obj : scene.objects)
        {
            if (auto * r = dynamic_cast<PointLight*>(obj.get())) renderer->add_light(r->data);
        }

        // Gather Objects
        std::vector<Renderable *> sceneObjects;
        for (auto & obj : scene.objects)
        {
            if (auto * r = dynamic_cast<Renderable*>(obj.get())) sceneObjects.push_back(r);
        }
        renderer->add_objects(sceneObjects);

        renderer->render_frame();

        gl_check_error(__FILE__, __LINE__);

        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);

        glActiveTexture(GL_TEXTURE0);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, renderer->get_output_texture(TextureType::COLOR, 0));
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

        auto & program = wireframeHandle.get();

        program.bind();

        program.uniform("u_eyePos", cam.get_eye_point());
        program.uniform("u_viewProjMatrix", viewProjectionMatrix);

        for (auto obj : editor->get_selection())
        {
            if (auto * r = dynamic_cast<Renderable*>(obj))
            {
                auto modelMatrix = mul(obj->get_pose().matrix(), make_scaling_matrix(obj->get_scale()));
                program.uniform("u_modelMatrix", modelMatrix);
                r->draw();
            }
        }

        program.unbind();

        glEnable(GL_DEPTH_TEST);
    }

    igm->begin_frame();

    gui::imgui_menu_stack menu(*this, ImGui::GetIO().KeysDown);
    menu.app_menu_begin();
    {
        menu.begin("File");
        if (menu.item("Open Scene", GLFW_MOD_CONTROL, GLFW_KEY_O))
        {

        }
        if (menu.item("Save Scene", GLFW_MOD_CONTROL, GLFW_KEY_S)) 
        {
            write_file_text("../assets/scene.json", cereal::serialize_to_json(scene.objects));
        }
        if (menu.item("New Scene", GLFW_MOD_CONTROL, GLFW_KEY_N)) 
        {
            scene.objects.clear();
        }
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

        menu.begin("Spawn");
        visit_subclasses((GameObject*)0, [&](const char * name, auto * p)
        {
            if (menu.item(name))
            {
                auto obj = std::make_shared<std::remove_reference_t<decltype(*p)>>();
                obj->set_material("default-material");
                scene.objects.push_back(obj);
            }
        });
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

    // Define a split region between the whole window and the left panel
    static int leftSplit = 380;
    static int leftSplit1 = (height / 2);
    auto leftRegionSplit = ImGui::Split({ { 0.f,17.f },{ (float)width, (float)height } }, &leftSplit, ImGui::SplitType::Left);
    auto lsplit2 = ImGui::Split(leftRegionSplit.second, &leftSplit1, ImGui::SplitType::Top);
    ui_rect topLeftPane = { { int2(lsplit2.second.min()) },{ int2(lsplit2.second.max()) } };
    ui_rect bottomLeftPane = { { int2(lsplit2.first.min()) },{ int2(lsplit2.first.max()) } };

    gui::imgui_fixed_window_begin("Renderer", topLeftPane);
    {

        ImGui::Separator();

        if (ImGui::TreeNode("Core"))
        {
            RendererSettings lastSettings = renderer->settings;

            if (Edit("renderer", *renderer))
            {
                renderer->gpuProfiler.set_enabled(renderer->settings.performanceProfiling);
                renderer->cpuProfiler.set_enabled(renderer->settings.performanceProfiling);

                if (renderer->settings.shadowsEnabled != lastSettings.shadowsEnabled)
                {
                    auto & shaderAsset = shaderMonitor.get_asset(pbrProgramAsset);
                    auto & defines = shaderAsset.defines;

                    if (renderer->settings.shadowsEnabled)
                    {
                        // Check if it's already in there
                        auto itr = std::find(defines.begin(), defines.end(), "ENABLE_SHADOWS");
                        if (itr == defines.end()) defines.push_back("ENABLE_SHADOWS");
                    }
                    else
                    {
                        auto & defines = shaderMonitor.get_asset(pbrProgramAsset).defines;
                        auto itr = std::find(defines.begin(), defines.end(), "ENABLE_SHADOWS");
                        if (itr != defines.end()) defines.erase(itr);
                    }
                    shaderAsset.shouldRecompile = true;
                }
            }

            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Procedural Sky"))
        {
            InspectGameObjectPolymorphic(nullptr, renderer->get_procedural_sky());
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Bloom + Tonemap"))   
        {
            Edit("bloom", *renderer->get_bloom_pass());
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Cascaded Shadow Mapping"))
        {
            Edit("shadows", *renderer->get_shadow_pass());
            ImGui::TreePop();
        }

        ImGui::Separator();

        if (renderer->settings.performanceProfiling)
        {
            for (auto & t : renderer->gpuProfiler.dataPoints) ImGui::Text("[GPU] %s %f ms", t.first.c_str(), (float)compute_mean(t.second.average));
            for (auto & t : renderer->cpuProfiler.dataPoints) ImGui::Text("[CPU] %s %f ms", t.first.c_str(), (float)compute_mean(t.second.average));
        }
    }
    gui::imgui_fixed_window_end();

    gui::imgui_fixed_window_begin("Application Log", bottomLeftPane);
    {
        log.Draw("-");
    }
    gui::imgui_fixed_window_end();

    igm->end_frame();

    // Debug Views
    /*
    {
        glViewport(0, 0, width, height);
        glDisable(GL_DEPTH_TEST);
        debugViews[0]->draw(uiSurface.children[0]->bounds, float2(width, height), renderer->get_output_texture(0));
        debugViews[1]->draw(uiSurface.children[1]->bounds, float2(width, height), renderer->get_output_texture(TextureType::DEPTH, 0));
        glEnable(GL_DEPTH_TEST);
    }
    */

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