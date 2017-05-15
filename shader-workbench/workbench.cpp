#include "index.hpp"
#include "workbench.hpp"

using namespace avl;

Geometry make_perlin_mesh(int gridSize = 32.f)
{
    Geometry terrain;

    for (int x = 0; x <= gridSize; x++)
    {
        for (int z = 0; z <= gridSize; z++)
        {
            float y = ((noise::noise(float2(x * 0.1f, z * 0.1f))) + 1.0f) / 2.0f;
            y = y * 2.0f;
            terrain.vertices.push_back({ (float)x, (float) y, (float)z });
        }
    }

    std::vector<uint4> quads;
    for (int x = 0; x < gridSize; ++x)
    {
        for (int z = 0; z < gridSize; ++z)
        {
            std::uint32_t tlIndex = z * (gridSize + 1) + x;
            std::uint32_t trIndex = z * (gridSize + 1) + (x + 1);
            std::uint32_t blIndex = (z + 1) * (gridSize + 1) + x;
            std::uint32_t brIndex = (z + 1) * (gridSize + 1) + (x + 1);
            quads.push_back({ blIndex, tlIndex, trIndex, brIndex });
        }
    }

    for (auto f : quads)
    {
        terrain.faces.push_back(uint3(f.x, f.y, f.z));
        terrain.faces.push_back(uint3(f.x, f.z, f.w));
    }

    terrain.compute_normals();

    return terrain;
}

shader_workbench::shader_workbench() : GLFWApp(1200, 800, "Shader Workbench")
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    igm.reset(new gui::ImGuiManager(window));
    gui::make_dark_theme();

    holoScanShader = shaderMonitor.watch("../assets/shaders/terrainscan_vert.glsl", "../assets/shaders/terrainscan_frag.glsl");
    normalDebug = shaderMonitor.watch("../assets/shaders/normal_debug_vert.glsl", "../assets/shaders/normal_debug_frag.glsl");

    //holoScanShader = shaderMonitor.watch("../assets/shaders/holoscan_vert.glsl", "../assets/shaders/holoscan_frag.glsl");

    terrainMesh = make_mesh_from_geometry(make_perlin_mesh(8));
    fullscreenQuad = make_fullscreen_quad();

    sceneColorTexture.setup(width, height, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    sceneDepthTexture.setup(width, height, GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glNamedFramebufferTexture2DEXT(sceneFramebuffer, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneColorTexture, 0);
    glNamedFramebufferTexture2DEXT(sceneFramebuffer, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, sceneDepthTexture, 0);
    sceneFramebuffer.check_complete();

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
    elapsedTime += e.timestep_ms;
}

void shader_workbench::on_draw() 
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    gpuTimer.start();

    //glEnable(GL_CULL_FACE);
    //glEnable(GL_DEPTH_TEST);
    //glEnable(GL_BLEND);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    const float4x4 projectionMatrix = cam.get_projection_matrix((float)width / (float)height);
    const float4x4 viewMatrix = cam.get_view_matrix();
    const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);

    // Main Scene
    {

        glBindFramebuffer(GL_FRAMEBUFFER, sceneFramebuffer);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float4x4 terrainModelMatrix = make_translation_matrix({ -4, 0, -4 });

        normalDebug->bind();

        normalDebug->uniform("u_viewProj", viewProjectionMatrix);
        normalDebug->uniform("u_modelMatrix", terrainModelMatrix);
        normalDebug->uniform("u_modelMatrixIT", inv(transpose(terrainModelMatrix)));

        terrainMesh.draw_elements();

        normalDebug->unbind();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Screenspace Effect
    {
        holoScanShader->bind();

        holoScanShader->uniform("u_time", elapsedTime);
        holoScanShader->uniform("u_eye", cam.get_eye_point());

        holoScanShader->uniform("u_scanDistance", scanDistance);
        holoScanShader->uniform("u_scanWidth", scanWidth);
        holoScanShader->uniform("u_leadSharp", leadSharp);
        holoScanShader->uniform("u_leadColor", leadColor);
        holoScanShader->uniform("u_midColor", midColor);
        holoScanShader->uniform("u_trailColor", trailColor);
        holoScanShader->uniform("u_hbarColor", hbarColor);

        holoScanShader->texture("s_colorTex", 0, sceneColorTexture, GL_TEXTURE_2D);
        holoScanShader->texture("s_depthTex", 1, sceneDepthTexture, GL_TEXTURE_2D);

        fullscreenQuad.draw_elements();

        holoScanShader->unbind();
    }

    //glDisable(GL_BLEND);

    gpuTimer.stop();

    igm->begin_frame();
    ImGui::Text("Render Time %f ms", gpuTimer.elapsed_ms());
    ImGui::SliderFloat("Scan Distance", &scanDistance, 0.1f, 10.f);
    ImGui::SliderFloat("Scan Width", &scanWidth, 0.1f, 10.f);
    ImGui::SliderFloat("Lead Sharp", &leadSharp, 0.1f, 10.f);
    ImGui::SliderFloat4("Lead Color", &leadColor.x, 0.0f, 1.f);
    ImGui::SliderFloat4("Mid Color", &midColor.x, 0.0f, 1.f);
    ImGui::SliderFloat4("Trail Color", &trailColor.x, 0.0f, 1.f);
    ImGui::SliderFloat4("hbarColor Color", &hbarColor.x, 0.0f, 1.f);
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
