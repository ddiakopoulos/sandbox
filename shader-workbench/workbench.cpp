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
            y = y * 3.0f;
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

    terrain.compute_normals(false);

    return terrain;
}

inline GlMesh fullscreen_quad_extra(const float4x4 & projectionMatrix, const float4x4 & viewMatrix)
{
    // Extract the frustum coordinates of the far clip plane
    float4 frustumVerts[4] = {
        { -1.f, -1.f, 1.f, 1.f }, // bottom left
        { -1.f, +1.f, 1.f, 1.f }, // bottom right
        { +1.f, +1.f, 1.f, 1.f }, // top right
        { +1.f, -1.f, 1.f, 1.f }  // top left
    };

    for (unsigned int j = 0; j < 4; ++j)
    {
        frustumVerts[j] = normalize(float4(transform_coord(inverse(mul(projectionMatrix, viewMatrix)), frustumVerts[j].xyz()), 1));
    }

    GlMesh mesh;

    struct Vertex { float3 position; float2 texcoord; float3 ray; };
    const float3 verts[6] = { { -1.0f, -1.0f, 0.0f },{ 1.0f, -1.0f, 0.0f },{ -1.0f, 1.0f, 0.0f },{ -1.0f, 1.0f, 0.0f },{ 1.0f, -1.0f, 0.0f },{ 1.0f, 1.0f, 0.0f } };
    const float2 texcoords[6] = { { 0, 0 },{ 1, 0 },{ 0, 1 },{ 0, 1 },{ 1, 0 },{ 1, 1 } };
    const float3 rayCoords[6] = { frustumVerts[0].xyz(), frustumVerts[3].xyz(),frustumVerts[1].xyz(),frustumVerts[1].xyz(),frustumVerts[3].xyz(), frustumVerts[2].xyz() };
    const uint3 faces[2] = { { 0, 1, 2 },{ 3, 4, 5 } };
    std::vector<Vertex> vertices;
    for (int i = 0; i < 6; ++i) vertices.push_back({ verts[i], texcoords[i], rayCoords[i] });

    mesh.set_vertices(vertices, GL_STATIC_DRAW);
    mesh.set_attribute(0, &Vertex::position);
    mesh.set_attribute(1, &Vertex::texcoord);
    mesh.set_attribute(2, &Vertex::ray);
    mesh.set_elements(faces, GL_STATIC_DRAW);

    return mesh;
}

shader_workbench::shader_workbench() : GLFWApp(1200, 800, "Shader Workbench")
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    igm.reset(new gui::ImGuiManager(window));
    gui::make_dark_theme();

    terrainScan = shaderMonitor.watch("../assets/shaders/terrainscan_vert.glsl", "../assets/shaders/terrainscan_frag.glsl");
    normalDebug = shaderMonitor.watch("../assets/shaders/normal_debug_vert.glsl", "../assets/shaders/normal_debug_frag.glsl");
    triplanarTexture = shaderMonitor.watch("../assets/shaders/triplanar_vert.glsl", "../assets/shaders/triplanar_frag.glsl");

    terrainMesh = make_mesh_from_geometry(make_perlin_mesh(16));

    sceneColorTexture.setup(width, height, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    sceneDepthTexture.setup(width, height, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glNamedFramebufferTexture2DEXT(sceneFramebuffer, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneColorTexture, 0);
    glNamedFramebufferTexture2DEXT(sceneFramebuffer, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, sceneDepthTexture, 0);
    sceneFramebuffer.check_complete();

    rustyTexture = load_image("../assets/textures/pbr/rusted_iron_2048/albedo.png", true);
    topTexture = load_image("../assets/nonfree/diffuse.png", true);
    glTextureParameteriEXT(rustyTexture, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteriEXT(rustyTexture, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTextureParameteriEXT(topTexture, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteriEXT(topTexture, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

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

static float3 scale = float3(0.25);

void shader_workbench::on_draw() 
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    gpuTimer.start();

    //glEnable(GL_BLEND);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    //glBlendEquation(GL_FUNC_ADD);

    const float4x4 projectionMatrix = cam.get_projection_matrix((float)width / (float)height);
    const float4x4 viewMatrix = cam.get_view_matrix();
    const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);

    fullscreenQuad = fullscreen_quad_extra(projectionMatrix, viewMatrix);

    // Main Scene
    {
        glEnable(GL_DEPTH_TEST);

        glBindFramebuffer(GL_FRAMEBUFFER, sceneFramebuffer);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float4x4 terrainModelMatrix = make_translation_matrix({ -8, 0, -8 });

        triplanarTexture->bind();

        triplanarTexture->uniform("u_viewProj", viewProjectionMatrix);
        triplanarTexture->uniform("u_modelMatrix", terrainModelMatrix);
        triplanarTexture->uniform("u_modelMatrixIT", inv(transpose(terrainModelMatrix)));
        triplanarTexture->texture("s_diffuseTextureA", 0, rustyTexture, GL_TEXTURE_2D);
        triplanarTexture->texture("s_diffuseTextureB", 1, topTexture, GL_TEXTURE_2D);
        triplanarTexture->texture("s_diffuseTextureB", 1, topTexture, GL_TEXTURE_2D);
        triplanarTexture->uniform("u_scale", scale);
        terrainMesh.draw_elements();

        triplanarTexture->unbind();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Screenspace Effect
    {
        glDisable(GL_DEPTH_TEST);

        terrainScan->bind();

        terrainScan->uniform("u_time", elapsedTime);
        terrainScan->uniform("u_eye", cam.get_eye_point());

        float4x4 terrainModelMatrix = make_translation_matrix({ -8, 0, -8 });

        terrainScan->uniform("u_inverseViewProjection", inverse(viewProjectionMatrix));

        terrainScan->uniform("u_scanDistance", scanDistance);
        terrainScan->uniform("u_scanWidth", scanWidth);
        terrainScan->uniform("u_leadSharp", leadSharp);
        terrainScan->uniform("u_leadColor", leadColor);
        terrainScan->uniform("u_midColor", midColor);
        terrainScan->uniform("u_trailColor", trailColor);
        terrainScan->uniform("u_hbarColor", hbarColor);

        terrainScan->texture("s_colorTex", 0, sceneColorTexture, GL_TEXTURE_2D);
        terrainScan->texture("s_depthTex", 1, sceneDepthTexture, GL_TEXTURE_2D);

        fullscreenQuad.draw_elements();

        terrainScan->unbind();
    }    
    
    //glDisable(GL_BLEND);

    gpuTimer.stop();

    igm->begin_frame();
    ImGui::Text("Render Time %f ms", gpuTimer.elapsed_ms());
    ImGui::SliderFloat3("Triplanar Scale", &scale.x, 0.0f, 1.f);
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
