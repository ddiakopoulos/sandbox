#include "index.hpp"
#include "workbench.hpp"

using namespace avl;

constexpr const char basic_vert[] = R"(#version 330
    layout(location = 0) in vec3 vertex;
    uniform mat4 u_mvp;
    void main()
    {
        gl_Position = u_mvp * vec4(vertex.xyz, 1);
    }
)";

constexpr const char basic_frag[] = R"(#version 330
    out vec4 f_color;
    uniform vec3 u_color;
    void main()
    {
        f_color = vec4(u_color, 1);
    }
)";

shader_workbench::shader_workbench() : GLFWApp(1200, 800, "Shader Workbench")
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    igm.reset(new gui::ImGuiManager(window));
    gui::make_dark_theme();

    basicShader = std::make_shared<GlShader>(basic_vert, basic_frag);

    wireframeShader = shaderMonitor.watch("../assets/shaders/wireframe_vert.glsl", "../assets/shaders/wireframe_frag.glsl", "../assets/shaders/wireframe_geom.glsl");

    holoScanShader = shaderMonitor.watch("../assets/shaders/holoscan_vert.glsl", "../assets/shaders/holoscan_frag.glsl");

    terrainMesh = make_plane_mesh(8, 8, 128, 128);

    auto shaderball = load_geometry_from_ply("../assets/models/sofa/sofa.ply");
    //rescale_geometry(shaderball, 1.f);
    for (auto & v : shaderball.vertices)
    {
        v = transform_coord(make_rotation_matrix({ 1, 0, 0 }, ANVIL_PI / 2), v);
        v *= 0.001f;
    }

    auto headset = load_geometry_from_ply("../assets/models/sofa/headset.ply");
    for (auto & v : headset.vertices)
    {
        v *= 0.0015f;
    }

    sofaMesh = make_mesh_from_geometry(shaderball);
    headsetMesh = make_mesh_from_geometry(headset);
    frustumMesh = make_frustum_mesh();

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

    if (event.type == InputEvent::KEY)
    {
        if (event.value[0] == GLFW_KEY_ESCAPE && event.action == GLFW_RELEASE) exit();
    }
}

void shader_workbench::on_update(const UpdateEvent & e) 
{
    flycam.update(e.timestep_ms);
    shaderMonitor.handle_recompile();
    elapsedTime += e.elapsed_s;
}

void shader_workbench::on_draw() 
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    gpuTimer.start();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(0.33f, 0.33f, 0.33f, 1.0f);

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    const float4x4 projectionMatrix = cam.get_projection_matrix((float)width / (float)height);
    const float4x4 viewMatrix = cam.get_view_matrix();
    const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);

    {
        float4x4 terrainModelMatrix = make_rotation_matrix({ 1, 0, 0 }, -ANVIL_PI / 2);

        holoScanShader->bind();
        holoScanShader->uniform("u_time", elapsedTime);
        holoScanShader->uniform("u_eye", cam.get_eye_point());
        holoScanShader->uniform("u_viewProj", viewProjectionMatrix);
        holoScanShader->uniform("u_modelMatrix", terrainModelMatrix);
        holoScanShader->uniform("u_modelMatrixIT", inv(transpose(terrainModelMatrix)));
        holoScanShader->uniform("u_triangleScale", triangleScale);
        holoScanShader->uniform("u_lightColor", float3(0.25));
        terrainMesh.draw_elements();

        holoScanShader->uniform("u_lightColor", float3(0.50));
        sofaMesh.draw_elements();

        float4x4 headsetModel = mul(make_rotation_matrix({ 0, 1, 0 }, ANVIL_PI), make_translation_matrix({ 1.5, 1.25, -1.9f }));
        holoScanShader->uniform("u_modelMatrix", headsetModel);
        holoScanShader->uniform("u_lightColor", float3(1.0));
        headsetMesh.draw_elements();

        holoScanShader->unbind();
    }

    {
        basicShader->bind();

        float4x4 frustumMeshModel = mul(make_rotation_matrix({ 1, 0, 0 }, -ANVIL_PI / 12), make_translation_matrix({ -0.025f, 0.825f, 1.925f }));
        basicShader->uniform("u_mvp", mul(viewProjectionMatrix, frustumMeshModel));
        basicShader->uniform("u_color", float3(1, 1, 1));
        frustumMesh.draw_elements();

        basicShader->unbind();
    }

    gpuTimer.stop();

    igm->begin_frame();
    ImGui::Text("Render Time %f ms", gpuTimer.elapsed_ms());
    ImGui::SliderFloat("Triangle Scale", &triangleScale, 0.1f, 1.0f);
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
