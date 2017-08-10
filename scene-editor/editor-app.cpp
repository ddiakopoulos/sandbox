#include "index.hpp"
#include "editor-app.hpp"

using namespace avl;

struct object
{
    Pose p;
};

class editor_controller
{
    GlGizmo gizmo;

    std::vector<object *> selected_objects;     // Array of selected objects
    std::vector<Pose> relative_transforms;      // Pose of the objects relative to the selection
    tinygizmo::rigid_transform selection;       // Center of mass of multiple objects or the pose of a single object

public:

    editor_controller()
    {

    }

    void on_input(const InputEvent & event)
    {
        gizmo.handle_input(event);
    }

    void on_update(const GlCamera & camera, const float2 viewport_size)
    {
        gizmo.update(camera, viewport_size);
        tinygizmo::transform_gizmo("editor-controller", gizmo.gizmo_ctx, selection);
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

    if (event.type == InputEvent::KEY)
    {
        if (event.value[0] == GLFW_KEY_ESCAPE && event.action == GLFW_RELEASE) exit();
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
