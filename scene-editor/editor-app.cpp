#include "index.hpp"
#include "editor-app.hpp"

using namespace avl;

struct object
{
    Pose pose;
};

inline Pose to_linalg(tinygizmo::rigid_transform & t)
{
    return {reinterpret_cast<float4 &>(t.orientation), reinterpret_cast<float3 &>(t.position) };
}

class editor_controller
{
    GlGizmo gizmo;
    tinygizmo::rigid_transform gizmo_selection;     // Center of mass of multiple objects or the pose of a single object

    Pose selection;
    std::vector<object *> selected_objects;         // Array of selected objects
    std::vector<Pose> relative_transforms;          // Pose of the objects relative to the selection

    void compute_selection()
    {
        if (selected_objects.size() == 0) selection = {};
        else if (selected_objects.size() == 1) selection = (*selected_objects.begin())->pose;
        else
        {
            // todo: orientation... bounding boxes?
            float3 center_of_mass = {};
            for (auto obj : selected_objects) center_of_mass += obj->pose.position;
            center_of_mass /= float3(selected_objects.size());
            selection.position = center_of_mass;
        }

        compute_relative_transforms();
    }

    void compute_relative_transforms()
    {
        relative_transforms.clear(); 

        for (auto s : selected_objects)
        {
            auto t = s->pose;
            relative_transforms.push_back(selection.inverse() * s->pose);
        }
    }

public:

    editor_controller()
    {

    }

    bool selected(object & object) const 
    { 
        return std::find(selected_objects.begin(), selected_objects.end(), &object) != selected_objects.end(); 
    }

    void clear() 
    { 
        selected_objects.clear();
    }

    const std::vector<object *> & get_objects() const 
    { 
        return selected_objects;
    }

    void set_selection(const std::vector<object *> & new_selection) 
    { 
        selected_objects = new_selection;
    }

    void on_input(const InputEvent & event)
    {
        gizmo.handle_input(event);
        selection = to_linalg(gizmo_selection);
    }

    void on_update(const GlCamera & camera, const float2 viewport_size)
    {
        gizmo.update(camera, viewport_size);
        tinygizmo::transform_gizmo("editor-controller", gizmo.gizmo_ctx, gizmo_selection);
    }

    void on_draw()
    {
        gizmo.draw();
    }
};

scene_editor_app::scene_editor_app() : GLFWApp(1280, 800, "Scene Editor")
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    igm.reset(new gui::ImGuiManager(window));
    gui::make_dark_theme();

    cam.look_at({ 0, 9.5f, -6.0f }, { 0, 0.1f, 0 });
    flycam.set_camera(&cam);

    wireframeShader = shaderMonitor.watch("../assets/shaders/wireframe_vert.glsl", "../assets/shaders/wireframe_frag.glsl", "../assets/shaders/wireframe_geom.glsl");
   
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

    // Prevent scene editor from responding to input destined for ImGui
    if (!ImGui::GetIO().WantCaptureMouse && !ImGui::GetIO().WantCaptureKeyboard)
    {
        if (event.type == InputEvent::KEY)
        {
            if (event.value[0] == GLFW_KEY_ESCAPE && event.action == GLFW_RELEASE)
            {
                exit();
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

    // tinygizmo::transform_gizmo("destination", gizmo->gizmo_ctx, destination);

    glViewport(0, 0, width, height);
    glClearColor(0.6f, 0.6f, 0.6f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const float4x4 projectionMatrix = cam.get_projection_matrix(float(width) / float(height));
    const float4x4 viewMatrix = cam.get_view_matrix();
    const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);

    gpuTimer.stop();

    // Render the scene
    {
        wireframeShader->bind();
        wireframeShader->uniform("u_eyePos", cam.get_eye_point());
        wireframeShader->uniform("u_viewProjMatrix", viewProjectionMatrix);

        for (auto & obj : objects)
        {
            wireframeShader->uniform("u_modelMatrix", obj.get_pose().matrix());
            obj.draw();
        }

        wireframeShader->unbind();
    }

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
