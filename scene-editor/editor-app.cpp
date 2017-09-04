#include "index.hpp"
#include "editor-app.hpp"
#include "../virtual-reality/material.cpp" // HACK HACK

using namespace avl;

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

    {
        // MetallicRoughnessMaterial and Material are two distinct types to the asset handle system. Hmm. 

        //MetallicRoughnessMaterial mat("material-move-test");
        //global_register_asset("material-move-test", std::move(mat));
        //Material & handle = MaterialHandle("material-move-test").get();
        //std::cout << "Handle: " << handle.id() << std::endl;
    }

    skybox.reset(new HosekProceduralSky());

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
        {"TWO_CASCADES", 
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
    renderer->set_procedural_sky(skybox.get());

    lightA.reset(new PointLight());
    lightA->data.color = float3(0.88f, 0.85f, 0.97f);
    lightA->data.position = float3(-5, 5, 0);
    lightA->data.radius = 12.f;
    objects.push_back(lightA);

    lightB.reset(new PointLight());
    lightB->data.color = float3(0.67f, 1.00f, 0.85f);
    lightB->data.position = float3(+5, 5, 0);
    lightB->data.radius = 12.f;
    objects.push_back(lightB);

    global_register_asset("rusted-iron-albedo", load_image("../assets/nonfree/Metal_ModernMetalIsoDiamondTile_2k_basecolor.tga", false));
    global_register_asset("rusted-iron-normal", load_image("../assets/nonfree/Metal_ModernMetalIsoDiamondTile_2k_n.tga", false));
    global_register_asset("rusted-iron-metallic", load_image("../assets/nonfree/Metal_ModernMetalIsoDiamondTile_2k_metallic.tga", false));
    global_register_asset("rusted-iron-roughness", load_image("../assets/nonfree/Metal_ModernMetalIsoDiamondTile_2k_roughness.tga", false));
    global_register_asset("rusted-iron-occlusion", load_image("../assets/nonfree/Metal_ModernMetalIsoDiamondTile_2k_ao.tga", false));
    //global_register_asset("rusted-iron-height", load_image("../assets/nonfree/Metal_RepaintedSteel_2k_h.tga", false));

    auto radianceBinary = read_file_binary("../assets/textures/envmaps/wells_radiance.dds");
    auto irradianceBinary = read_file_binary("../assets/textures/envmaps/wells_irradiance.dds");
    gli::texture_cube radianceHandle(gli::load_dds((char *)radianceBinary.data(), radianceBinary.size()));
    gli::texture_cube irradianceHandle(gli::load_dds((char *)irradianceBinary.data(), irradianceBinary.size()));
    global_register_asset("wells-radiance-cubemap", load_cubemap(radianceHandle));
    global_register_asset("wells-irradiance-cubemap", load_cubemap(irradianceHandle));

    auto ico = make_icosasphere(5);
    global_register_asset("icosphere", make_mesh_from_geometry(ico));
    global_register_asset("icosphere", std::move(ico));

    MetallicRoughnessMaterial pbrMaterialInstance("pbr-forward-lighting");
    std::cout << "Setting: " << pbrMaterialInstance.id() << std::endl;

    pbrMaterialInstance.set_albedo_texture("rusted-iron-albedo");
    pbrMaterialInstance.set_normal_texture("rusted-iron-normal");
    pbrMaterialInstance.set_metallic_texture("rusted-iron-metallic");
    pbrMaterialInstance.set_roughness_texture("rusted-iron-roughness");
    pbrMaterialInstance.set_height_texture("rusted-iron-height");
    pbrMaterialInstance.set_occulusion_texture("rusted-iron-occlusion");
    pbrMaterialInstance.set_radiance_cubemap("wells-radiance-cubemap");
    pbrMaterialInstance.set_irrradiance_cubemap("wells-irradiance-cubemap");
    //pbrMaterialInstance.set_roughness(remap<float>(i, 0.0f, 5.0f, 0.0f, 1.f));
    //pbrMaterialInstance.set_metallic(remap<float>(j, 0.0f, 5.0f, 0.0f, 1.f));
    global_register_asset("some-material-instance", std::move(pbrMaterialInstance));

    for (int i = 0; i < 6; ++i)
    {
        for (int j = 0; j < 6; ++j)
        {
            StaticMesh mesh(GlMeshHandle("icosphere"), GeometryHandle("icosphere"));
            Pose p;
            p.position = float3((i * 2) - 5, 0, (j * 2) - 5);
            mesh.set_pose(p);
            mesh.set_material(&MetallicRoughnessMaterialHandle("some-material-instance").get());
            objects.push_back(std::make_shared<StaticMesh>(std::move(mesh)));
        }
    }

    auto cube = make_cube();
    global_register_asset("cube", make_mesh_from_geometry(cube));
    global_register_asset("cube", std::move(cube));

    StaticMesh floorMesh(GlMeshHandle("cube"), GeometryHandle("cube"));
    floorMesh.set_pose(Pose(float3(0, -2.01f, 0)));
    floorMesh.set_scale(float3(16, 0.1f, 16));
    floorMesh.set_material(&MetallicRoughnessMaterialHandle("some-material-instance").get()); // ptr

    std::cout << "Getting: " << MetallicRoughnessMaterialHandle("some-material-instance").get().id() << std::endl;

    std::shared_ptr<StaticMesh> floor = std::make_shared<StaticMesh>(std::move(floorMesh));
    objects.push_back(floor);

    auto output = cereal::ToJson(floor);
    write_file_text("floor-object.json", output);

    /*
    auto floorJson = read_file_text("floor-object.json");
    std::istringstream floorJsonStream(floorJson);
    cereal::JSONInputArchive inJson(floorJsonStream);
    std::shared_ptr<StaticMesh> floor;
    inJson(floor);
    objects.push_back(floor);
    */

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

                for (auto & obj : objects)
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
        renderer->add_light(&lightA->data);
        renderer->add_light(&lightB->data);

        // Gather Objects
        std::vector<Renderable *> sceneObjects;
        for (auto & obj : objects)
        {
            if (auto * rndr = dynamic_cast<Renderable*>(obj.get()))
            {
                sceneObjects.push_back(rndr);
            }
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
        //glDisable(GL_DEPTH_TEST);

        auto & program = AssetHandle<GlShader>("wireframe").get();

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

        //glEnable(GL_DEPTH_TEST);
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
            auto it = std::remove_if(begin(objects), end(objects), [this](std::shared_ptr<GameObject> obj) 
            { 
                return editor->selected(obj.get());
            });
            objects.erase(it, end(objects));
            editor->clear();
        }
        if (menu.item("Select All", GLFW_MOD_CONTROL, GLFW_KEY_A)) 
        {
            std::vector<GameObject *> selectedObjects;
            for (auto & obj : objects)
            {
                selectedObjects.push_back(obj.get());
            }
            editor->set_selection(selectedObjects);
        }
        menu.end();
    }
    menu.app_menu_end();

    gui::imgui_fixed_window_begin("Objects", { { width - 320, 17 }, { width, height } });

    for (size_t i = 0; i < objects.size(); ++i)
    {
        ImGui::PushID(static_cast<int>(i));
        bool selected = editor->selected(objects[i].get());
        std::string name = std::string(typeid(objects[i]).name());
        std::vector<StaticMesh *> selectedObjects;

        if (ImGui::Selectable(name.c_str(), &selected))
        {
            if (!ImGui::GetIO().KeyCtrl) editor->clear();
            editor->update_selection(objects[i].get());
        }
        ImGui::PopID();
    }

    gui::imgui_fixed_window_end();

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
