#include "index.hpp"
#include "workbench.hpp"

using namespace avl;

shader_workbench::shader_workbench() : GLFWApp(1200, 800, "Shader Workbench")
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    igm.reset(new gui::ImGuiManager(window));
    gui::make_dark_theme();

    holoScanShader = shaderMonitor.watch("../assets/shaders/holoscan_vert.glsl", "../assets/shaders/holoscan_frag.glsl");

    terrainMesh = make_plane_mesh(8, 8, 128, 128);

    cam.look_at({ 0, 3.0, -3.5 }, { 0, 2.0, 0 });
    flycam.set_camera(&cam);
}

shader_workbench::~shader_workbench()
{

}

void shader_workbench::on_window_resize(int2 size) 
{

}

void shader_workbench::on_input(const InputEvent & event) 
{
    igm->update_input(event);
    flycam.handle_input(event);
}

void shader_workbench::on_update(const UpdateEvent & e) 
{
    flycam.update(e.timestep_ms);
    shaderMonitor.handle_recompile();
}

void shader_workbench::on_draw() 
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(0.66f, 0.66f, 0.66f, 1.0f);

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    const float4x4 projectionMatrix = cam.get_projection_matrix((float)width / (float)height);
    const float4x4 viewMatrix = cam.get_view_matrix();
    const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);

    {
        float4x4 terrainModelMatrix = make_rotation_matrix({ 1, 0, 0 }, -ANVIL_PI / 2);

        holoScanShader->bind();
        holoScanShader->uniform("u_eye", cam.get_eye_point());
        holoScanShader->uniform("u_viewProj", viewProjectionMatrix);
        holoScanShader->uniform("u_modelMatrix", terrainModelMatrix);
        holoScanShader->uniform("u_modelMatrixIT", inv(transpose(terrainModelMatrix)));
        terrainMesh.draw_elements();
        holoScanShader->unbind();
    }

    igm->begin_frame();
    ImGui::Text("Controls");
    igm->end_frame();

    gl_check_error(__FILE__, __LINE__);

    glfwSwapBuffers(window);
}

IMPLEMENT_MAIN(int argc, char * argv[])
{
    try
    {
        shader_workbench app;
        app.main_loop();
    }
    catch (const std::exception & e)
    {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
