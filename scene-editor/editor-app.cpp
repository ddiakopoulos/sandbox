#include "index.hpp"
#include "editor-app.hpp"

using namespace avl;

scene_editor_app::scene_editor_app() : GLFWApp(1280, 800, "Scene Editor")
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    timer.start();

    igm.reset(new gui::ImGuiManager(window));
    gui::make_dark_theme();

    controller.reset(new editor_controller<SimpleStaticMesh>());

    cam.look_at({ 0, 9.5f, -6.0f }, { 0, 0.1f, 0 });
    flycam.set_camera(&cam);

    wireframeShader = shaderMonitor.watch("../assets/shaders/wireframe_vert.glsl", "../assets/shaders/wireframe_frag.glsl", "../assets/shaders/wireframe_geom.glsl");
    pbrShader = shaderMonitor.watch("../assets/shaders/textured_pbr_vert.glsl", "../assets/shaders/textured_pbr_frag.glsl");

    pbrMaterial.reset(new MetallicRoughnessMaterial(pbrShader));

    Geometry icosphere = make_icosasphere(3);
    float step = ANVIL_TAU / 8;
    for (int i = 0; i < 8; ++i)
    {
        SimpleStaticMesh mesh;
        mesh.set_static_mesh(icosphere);

        Pose p;
        p.position = float3(std::sin(step * i) * 5.0f, 0, std::cos(step * i) * 5.0f);
        mesh.set_pose(p);
        objects.push_back(std::move(mesh));
    }

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
    controller->on_input(event);

    // Prevent scene editor from responding to input destined for ImGui
    if (!ImGui::GetIO().WantCaptureMouse && !ImGui::GetIO().WantCaptureKeyboard)
    {
        if (event.type == InputEvent::KEY)
        {
            if (event.value[0] == GLFW_KEY_ESCAPE && event.action == GLFW_RELEASE)
            {
                // De-select all objects
                controller->clear();
            }
        }

        if (event.type == InputEvent::MOUSE && event.action == GLFW_PRESS && event.value[0] == GLFW_MOUSE_BUTTON_LEFT)
        {
            int width, height;
            glfwGetWindowSize(window, &width, &height);

            std::cout << "selectable: " << controller->has_edited() << std::endl;
            const Ray r = cam.get_world_ray(event.cursor, float2(width, height));

            if (length(r.direction) > 0 && !controller->has_edited())
            {
                std::vector<SimpleStaticMesh *> selectedObjects;
                for (auto & obj : objects)
                {
                    RaycastResult result = obj.raycast(r);
                    if (result.hit) selectedObjects.push_back(&obj); 
                }

                // New object was selected
                if (selectedObjects.size() > 0)
                {
                    // Multi-selection
                    if (event.mods & GLFW_MOD_CONTROL)
                    {
                        auto existingSelection = controller->get_selection();
                        for (auto s : selectedObjects)
                        {
                            if (!controller->selected(s)) existingSelection.push_back(s);
                        }
                        controller->set_selection(existingSelection);
                    }
                    // Single Selection
                    else
                    {
                        controller->set_selection(selectedObjects);
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
    controller->on_update(cam, float2(width, height));
}

void scene_editor_app::on_draw()
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    gpuTimer.start();

    glViewport(0, 0, width, height);
    glClearColor(0.6f, 0.6f, 0.6f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const float4x4 projectionMatrix = cam.get_projection_matrix(float(width) / float(height));
    const float4x4 viewMatrix = cam.get_view_matrix();
    const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);

    glBindBufferBase(GL_UNIFORM_BUFFER, uniforms::per_scene::binding, per_scene);
    glBindBufferBase(GL_UNIFORM_BUFFER, uniforms::per_view::binding, per_view);

    // per-scene uniform buffer
    {
        uniforms::per_scene b = {};
        b.time = timer.milliseconds().count();
        b.resolution = float2(width, height);
        b.invResolution = 1.f / b.resolution;

        //b.directional_light.color = lights.directionalLight->color;
        //b.directional_light.direction = lights.directionalLight->direction;
        //b.directional_light.amount = lights.directionalLight->amount;
        //b.activePointLights = 0;
        //for (int i = 0; i < (int)std::min(lights.pointLights.size(), size_t(4)); ++i) b.point_lights[i] = *lights.pointLights[i];

        per_scene.set_buffer_data(sizeof(b), &b, GL_STREAM_DRAW);
    }

    // per-scene uniform buffer
    {
        uniforms::per_view v = {};
        v.view = cam.get_pose().inverse().matrix();
        v.viewProj = viewProjectionMatrix;
        v.eyePos = float4(cam.get_pose().position, 1);

        //v.cascadesPlane[c] = float4(shadow->splitPlanes[c].x, shadow->splitPlanes[c].y, 0, 0);
        //v.cascadesMatrix[c] = shadow->shadowMatrices[c];
        //v.cascadesNear[c] = shadow->nearPlanes[c];
        //v.cascadesFar[c] = shadow->farPlanes[c];

        per_view.set_buffer_data(sizeof(v), &v, GL_STREAM_DRAW);
    }

    // Selected objects as wireframe
    {
        wireframeShader->bind();
        wireframeShader->uniform("u_eyePos", cam.get_eye_point());
        wireframeShader->uniform("u_viewProjMatrix", viewProjectionMatrix);

        for (auto obj : controller->get_selection())
        {
            wireframeShader->uniform("u_modelMatrix", obj->get_pose().matrix());
            obj->draw();
        }

        wireframeShader->unbind();
    }

    {
        EyeData eye;
        eye.pose = cam.get_pose();
        eye.viewMatrix = cam.get_pose().inverse().matrix();
        eye.projectionMatrix = projectionMatrix;
        eye.viewProjMatrix = viewProjectionMatrix;
           
        ShadowData shadow;
        shadow.csmArrayHandle = -1;
        shadow.directionalLight = float3(0, -1, 0);

        RenderPassData pd(0, eye, shadow);

        pbrMaterial->update_uniforms(&pd);

        for (auto & obj : objects)
        {
            pbrMaterial->use(obj.get_pose().matrix(), eye.viewMatrix);
            obj.draw();
        }
    }

    gpuTimer.stop();

    igm->begin_frame();

    ImGui::Text("Render Time %f ms", gpuTimer.elapsed_ms());

    gui::imgui_menu_stack menu(*this, igm->capturedKeys);
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
        if (menu.item("Delete", 0, GLFW_KEY_DELETE)) {}
        if (menu.item("Select All", GLFW_MOD_CONTROL, GLFW_KEY_A)) {}
        menu.end();
    }
    menu.app_menu_end();

    std::vector<std::string> fakeObjects = { "mesh", "floor", "skybox", "debug-camera", "cinematic-camera", "point-light", "point-light", "directional-light" };

    gui::imgui_fixed_window_begin("Objects", { { width - 320, 17 }, { width, height } });

    for (size_t i = 0; i < fakeObjects.size(); ++i)
    {
        ImGui::PushID(static_cast<int>(i));
        bool selected = false;
        std::string name = fakeObjects[i];
        if (ImGui::Selectable(name.c_str(), &selected))
        {

        }
        ImGui::PopID();
    }

    gui::imgui_fixed_window_end();

    igm->end_frame();

    // Scene editor gizmo
    glClear(GL_DEPTH_BUFFER_BIT);
    controller->on_draw();

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
    catch (...)
    {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
