#include "index.hpp"
#include "terrain-scan-effect.hpp"
#include <functional>

/* 
 * Re-implemented based on this tutorial: "No Man's Sky: Topographic Scanning"
 * https://www.youtube.com/watch?v=OKoNp2RqE9A 
 */

using namespace avl;

constexpr const char skybox_vert[] = R"(#version 330
    layout(location = 0) in vec3 vertex;
    layout(location = 1) in vec3 normal;
    uniform mat4 u_viewProj;
    uniform mat4 u_modelMatrix;
    out vec3 v_normal;
    out vec3 v_world;
    void main()
    {
        vec4 worldPosition = u_modelMatrix * vec4(vertex, 1);
        gl_Position = u_viewProj * worldPosition;
        v_world = worldPosition.xyz;
        v_normal = normal;
    }
)";

constexpr const char skybox_frag[] = R"(#version 330
    in vec3 v_normal, v_world;
    in float u_time;
    out vec4 f_color;
    uniform vec3 u_bottomColor;
    uniform vec3 u_topColor;
    void main()
    {
        float h = normalize(v_world).y;
        f_color = vec4( mix( u_bottomColor, u_topColor, max( pow( max(h, 0.0 ), 0.8 ), 0.0 ) ), 1.0 );
    }
)";

Geometry make_perlin_terrain_mesh(int gridSize = 32.f)
{
    Geometry terrain;

    for (int x = 0; x <= gridSize; x++)
    {
        for (int z = 0; z <= gridSize; z++)
        {
            float y = ((noise::noise(float2(x * 0.1f, z * 0.1f))) + 1.0f) / 2.0f;
            y = y * 3.0f;
            terrain.vertices.push_back({ (float)x, (float)y, (float)z });
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

    compute_normals(terrain);

    return terrain;
}

inline GlMesh fullscreen_quad_extra(const float4x4 & projectionMatrix, const float4x4 & viewMatrix)
{
    // Camera position is reconstructed in the shader, but we still need the correct orientation 
    float4x4 viewMatrixNoTranslation = viewMatrix;
    viewMatrixNoTranslation.w = float4(0, 0, 0, 1);

    // Extract the frustum coordinates of the far clip plane in ndc space
    float3 frustumVerts[8] = {
        { -1.f, -1.f, 1.f },
        { -1.f, +1.f, 1.f },
        { +1.f, +1.f, 1.f },
        { +1.f, -1.f, 1.f },
    };

    for (unsigned int j = 0; j < 8; ++j) frustumVerts[j] = transform_coord(inverse(mul(projectionMatrix, viewMatrixNoTranslation)), frustumVerts[j]);

    struct Vertex { float3 position; float2 texcoord; float3 ray; };
    const float3 verts[6] = { { -1.0f, -1.0f, 0.0f },{ 1.0f, -1.0f, 0.0f },{ -1.0f, 1.0f, 0.0f },{ -1.0f, 1.0f, 0.0f },{ 1.0f, -1.0f, 0.0f },{ 1.0f, 1.0f, 0.0f } };
    const float2 texcoords[6] = { { 0, 0 },{ 1, 0 },{ 0, 1 },{ 0, 1 },{ 1, 0 },{ 1, 1 } };
    const float3 rayCoords[6] = { frustumVerts[0], frustumVerts[3], frustumVerts[1], frustumVerts[1], frustumVerts[3], frustumVerts[2] };
    const uint3 faces[2] = { { 0, 1, 2 },{ 3, 4, 5 } };
    std::vector<Vertex> vertices;
    for (int i = 0; i < 6; ++i) vertices.push_back({ verts[i], texcoords[i], rayCoords[i] });

    GlMesh mesh;
    mesh.set_vertices(vertices, GL_STATIC_DRAW);
    mesh.set_attribute(0, &Vertex::position);
    mesh.set_attribute(1, &Vertex::texcoord);
    mesh.set_attribute(2, &Vertex::ray);
    mesh.set_elements(faces, GL_STATIC_DRAW);
    return mesh;
}

shader_workbench::shader_workbench() : GLFWApp(1280, 720, "Terrain Scanning Effect")
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    igm.reset(new gui::imgui_wrapper(window));
    gui::make_light_theme();

    cam.look_at({ 0, 3.0, -3.5 }, { 0, 2.0, 0 });
    flycam.set_camera(&cam);

    gizmo.reset(new GlGizmo());

    sceneColorTexture.setup(width, height, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    sceneDepthTexture.setup(width, height, GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glNamedFramebufferTexture2DEXT(sceneFramebuffer, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneColorTexture, 0);
    glNamedFramebufferTexture2DEXT(sceneFramebuffer, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, sceneDepthTexture, 0);
    sceneFramebuffer.check_complete();

    shaderMonitor.watch("../assets/shaders/effects/terrainscan_vert.glsl", "../assets/shaders/effects/terrainscan_frag.glsl", [&](GlShader & shader)
    {
        scanningEffect = std::move(shader);
    });

    shaderMonitor.watch("../assets/shaders/effects/triplanar_vert.glsl", "../assets/shaders/effects/triplanar_frag.glsl", [&](GlShader & shader)
    {
        triplanarTerrain = std::move(shader);
    });

    skyShader = GlShader(skybox_vert, skybox_frag);

    skyMesh = make_sphere_mesh(1.0f);
    terrainMesh = make_mesh_from_geometry(make_perlin_terrain_mesh(32));

    grassTexture = load_image("../assets/textures/terrain-grass-diffuse.png", true);
    glTextureParameteriEXT(grassTexture, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteriEXT(grassTexture, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    rockTexture = load_image("../assets/textures/terrain-rock-diffuse.png", true);
    glTextureParameteriEXT(rockTexture, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteriEXT(rockTexture, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    destination.position.y = 3;
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
        if (event.value[0] == GLFW_KEY_SPACE && event.action == GLFW_RELEASE) take_screenshot("terrain-scan-effect");
    }

    if (gizmo) gizmo->handle_input(event);
}

void shader_workbench::on_update(const UpdateEvent & e)
{
    flycam.update(e.timestep_ms);
    shaderMonitor.handle_recompile();
    elapsedTime += e.timestep_ms;
    animator.update(e.timestep_ms);
}

void shader_workbench::on_draw()
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    if (gizmo) gizmo->update(cam, float2(width, height));

    gpuTimer.start();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const float4x4 projectionMatrix = cam.get_projection_matrix(float(width) / float(height));
    const float4x4 viewMatrix = cam.get_view_matrix();
    const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);

    fullscreenQuad = fullscreen_quad_extra(projectionMatrix, viewMatrix);

    tinygizmo::transform_gizmo("destination", gizmo->gizmo_ctx, destination);

    // Main Scene
    {
        glEnable(GL_DEPTH_TEST);

        glBindFramebuffer(GL_FRAMEBUFFER, sceneFramebuffer);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        skyShader.bind();
        float4x4 world = mul(make_translation_matrix(cam.get_eye_point()), scaling_matrix(float3(cam.farclip * .99f)));
        float4x4 mvp = mul(viewProjectionMatrix, world);
        skyShader.uniform("u_viewProj", viewProjectionMatrix);
        skyShader.uniform("u_modelMatrix", world);
        skyShader.uniform("u_bottomColor", float3(52.0f / 255.f, 62.0f / 255.f, 82.0f / 255.f));
        skyShader.uniform("u_topColor", float3(81.0f / 255.f, 101.0f / 255.f, 142.0f / 255.f));
        skyMesh.draw_elements();
        skyShader.unbind();

        const float4x4 terrainModelMatrix = make_translation_matrix({ -16, 0, -16 });

        triplanarTerrain.bind();
        triplanarTerrain.uniform("u_viewProj", viewProjectionMatrix);
        triplanarTerrain.uniform("u_modelMatrix", terrainModelMatrix);
        triplanarTerrain.uniform("u_modelMatrixIT", inverse(transpose(terrainModelMatrix)));
        triplanarTerrain.texture("s_diffuseTextureA", 0, grassTexture, GL_TEXTURE_2D);
        triplanarTerrain.texture("s_diffuseTextureB", 1, rockTexture, GL_TEXTURE_2D);
        triplanarTerrain.uniform("u_scale", triplanarTextureScale);
        terrainMesh.draw_elements();
        triplanarTerrain.unbind();
    }

    // Screenspace Scanning Effect
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glDisable(GL_DEPTH_TEST);

        scanningEffect.bind();

        scanningEffect.uniform("u_time", elapsedTime);
        scanningEffect.uniform("u_eye", cam.get_eye_point());
        scanningEffect.uniform("u_nearClip", cam.nearclip);
        scanningEffect.uniform("u_farClip", cam.farclip);

        scanningEffect.uniform("u_inverseViewProjection", inverse(viewProjectionMatrix));

        scanningEffect.uniform("u_scanDistance", ringDiameter);
        scanningEffect.uniform("u_scanWidth", ringEdgeSize);
        scanningEffect.uniform("u_leadSharp", ringEdgeSharpness);
        scanningEffect.uniform("u_leadColor", leadColor);
        scanningEffect.uniform("u_midColor", midColor);
        scanningEffect.uniform("u_trailColor", trailColor);
        scanningEffect.uniform("u_hbarColor", hbarColor);

        scanningEffect.texture("s_colorTex", 0, sceneColorTexture, GL_TEXTURE_2D);
        scanningEffect.texture("s_depthTex", 1, sceneDepthTexture, GL_TEXTURE_2D);

        scanningEffect.uniform("u_scannerPosition", float3(destination.position.x, destination.position.y, destination.position.z));

        fullscreenQuad.draw_elements();

        scanningEffect.unbind();
    }

    gpuTimer.stop();

    igm->begin_frame();
    ImGui::Text("Render Time %f ms", gpuTimer.elapsed_ms());
    ImGui::Separator();
    ImGui::SliderFloat3("Triplanar Scale", &triplanarTextureScale.x, 0.0f, 1.f);
    ImGui::Separator();
    ImGui::SliderFloat("Scanning Ring Diameter", &ringDiameter, 0.1f, 10.f);
    ImGui::SliderFloat("Scanning Ring Edge Size", &ringEdgeSize, 0.1f, 10.f);
    ImGui::SliderFloat("Scanning Ring Edge Sharpness", &ringEdgeSharpness, 0.1f, 10.f);
    ImGui::SliderFloat4("Ring Outer Color", &leadColor.x, 0.0f, 1.f);
    ImGui::SliderFloat4("Ring Middle Color", &midColor.x, 0.0f, 1.f);
    ImGui::SliderFloat4("Ring Inner Color", &trailColor.x, 0.0f, 1.f);
    ImGui::SliderFloat4("Bar Color", &hbarColor.x, 0.0f, 1.f);
    ImGui::Separator();
    if (ImGui::Button("Scan"))
    {
        auto & tween = animator.add_tween(&ringDiameter, 32.f, 1.5f, Sine::ease_in_out);
        tween.on_finish = [&](void) { ringDiameter = 0.f; };
    }
    igm->end_frame();

    if (gizmo) gizmo->draw();

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
