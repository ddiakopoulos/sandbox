#include "index.hpp"
#include "workbench.hpp"
#include "imgui/imgui_internal.h"

using namespace avl;

/*
 * Blend Mode Reference
 * ==================================================================
 * glBlendFunc: SrcAlpha, OneMinusSrcAlpha     // Alpha blending
 * glBlendFunc: One, One                       // Additive
 * glBlendFunc: OneMinusDstColor, One          // Soft Additive
 * glBlendFunc: DstColor, Zero                 // Multiplicative
 * glBlendFunc: DstColor, SrcColor             // 2x Multiplicative
 * ==================================================================
 */

struct ProjectorControl
{
    tinygizmo::rigid_transform transform;
    GlMesh mesh = make_frustum_mesh(1.f);

    ProjectorControl()
    {
        Pose lookat = look_at_pose_rh(float3(1, 4, 0), float3(0, 0, 0));
        transform.position.x = lookat.position.x;
        transform.position.y = lookat.position.y;
        transform.position.z = lookat.position.z;
        transform.orientation.x = lookat.orientation.x;
        transform.orientation.y = lookat.orientation.y;
        transform.orientation.z = lookat.orientation.z;
        transform.orientation.w = lookat.orientation.w;
    }

    void draw_debug(const float4x4 & model, GlShader * shader, const float4x4 & viewProj, tinygizmo::gizmo_context & ctx, gl_material_projector & projector)
    {
        // Draw the Gizmo
        tinygizmo::transform_gizmo("projector", ctx, transform);

        // Extract native linalg float4x4 from tinygizmo
        const float4x4 view = reinterpret_cast<const float4x4 &>(transform.matrix());

        // Set the projector's camera matrix
        projector.viewMatrix = mul(view, model); // This transforms all of our vertex positions back to world space

        //const auto lookAtModelPosition = transform_coord(inverseModelMatrix, float3(2.f, 0.1f, 2.f));
        //const auto lightModelPosition = transform_coord(inverseModelMatrix, float3(-transform.position.x, -transform.position.y, -transform.position.z));

        //auto PoseInModelSpace = look_at_pose_rh(lightModelPosition, lookAtModelPosition);

        //projector.viewMatrix = PoseInModelSpace.matrix();

        // Draw debug visualization 
        shader->bind();
        shader->uniform("u_viewProj", viewProj);
        shader->uniform("u_modelMatrix", view);
        shader->uniform("u_modelMatrixIT", inv(transpose(view)));
        mesh.draw_elements();
        shader->unbind();
    }
};

Geometry make_perlin_mesh(int gridSize = 32.f)
{
    Geometry terrain;

    for (int x = 0; x <= gridSize; x++)
    {
        for (int z = 0; z <= gridSize; z++)
        {
            float y = ((noise::noise(float2(x * 0.1f, z * 0.1f))) + 1.0f) / 2.0f;
            y = y * 2.0f;
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

    terrain.compute_normals(false);

    return terrain;
}

std::unique_ptr<ProjectorControl> projectorController;

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
    projector.projectorMultiplyShader = shaderMonitor.watch("../assets/shaders/projector_multiply_vert.glsl", "../assets/shaders/projector_multiply_frag.glsl");

    terrainMesh = make_mesh_from_geometry(make_perlin_mesh(32));

    rustyTexture = load_image("../assets/textures/pbr/rusted_iron_2048/albedo.png", true);
    topTexture = load_image("../assets/nonfree/diffuse.png", true);
    glTextureParameteriEXT(rustyTexture, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteriEXT(rustyTexture, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTextureParameteriEXT(topTexture, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteriEXT(topTexture, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    projector.cookieTexture = std::make_shared<GlTexture2D>(load_image("../assets/textures/projector/hexagon_select.png", false));
    glTextureParameteriEXT(*projector.cookieTexture, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTextureParameteriEXT(*projector.cookieTexture, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    projector.gradientTexture = std::make_shared<GlTexture2D>(load_image("../assets/textures/projector/gradient.png", false));
    glTextureParameteriEXT(*projector.gradientTexture, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTextureParameteriEXT(*projector.gradientTexture, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    gizmo.reset(new GlGizmo());

    projectorController.reset(new ProjectorControl());

    //projector.pose = look_at_pose_rh({ 0.1f, 4.0, 0.1f }, { 0, 0.1f, 0 });
    std::cout << "Cookie: " << *projector.cookieTexture << std::endl;
    std::cout << "Gradient: " << *projector.gradientTexture << std::endl;

    cam.look_at({ 0, 9.5f, -6.0f }, { 0, 0.1f, 0 });
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

    if (gizmo) gizmo->handle_input(event);
}

void shader_workbench::on_update(const UpdateEvent & e)
{
    flycam.update(e.timestep_ms);
    shaderMonitor.handle_recompile();
    elapsedTime += e.timestep_ms;
}

const char * listbox_items[] = { "GL_ZERO", "GL_ONE", "GL_SRC_COLOR", "GL_ONE_MINUS_SRC_COLOR", "GL_DST_COLOR", "GL_ONE_MINUS_DST_COLOR", "GL_SRC_ALPHA", "GL_DST_ALPHA", "GL_ONE_MINUS_DST_ALPHA" };
std::vector<int> blendModes = { GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR, GL_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA };

static int src_blendmode = 0;
static int dst_blendmode = 6;
static bool renderColor = true;
static bool renderProjective = true;
static float3 scale = float3(0.25);

void shader_workbench::on_draw()
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    gpuTimer.start();

    const float4x4 projectionMatrix = cam.get_projection_matrix(float(width) / float(height));
    const float4x4 viewMatrix = cam.get_view_matrix();
    const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);
    if (gizmo) gizmo->update(cam, float2(width, height));

    float4x4 terrainModelMatrix = make_translation_matrix({ -16, 0, -16 });

    // Main Scene
    {
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);

        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (renderColor)
        {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            triplanarTexture->bind();
            triplanarTexture->uniform("u_viewProj", viewProjectionMatrix);
            triplanarTexture->uniform("u_modelMatrix", terrainModelMatrix);
            triplanarTexture->uniform("u_modelMatrixIT", inv(transpose(terrainModelMatrix)));
            triplanarTexture->texture("s_diffuseTextureA", 0, rustyTexture, GL_TEXTURE_2D);
            triplanarTexture->texture("s_diffuseTextureB", 1, topTexture, GL_TEXTURE_2D);
            triplanarTexture->uniform("u_scale", scale);
            terrainMesh.draw_elements();
            triplanarTexture->unbind();
        }

        // Draw a gizmo for the projector and visualize a frustum with it
        projectorController->draw_debug(terrainModelMatrix, normalDebug.get(), viewProjectionMatrix, gizmo->gizmo_ctx, projector);

        if (renderProjective)
        {
            //glDisable(GL_DEPTH_TEST);
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(-1.0, -1.0);
            glBlendFunc(blendModes[src_blendmode], blendModes[dst_blendmode]);

            projector.projectorMultiplyShader->bind();

            float4x4 projectorViewProj = projector.get_view_projection_matrix(false);

            projector.projectorMultiplyShader->uniform("u_time", elapsedTime);
            projector.projectorMultiplyShader->uniform("u_eye", cam.get_eye_point());
            projector.projectorMultiplyShader->uniform("u_viewProj", viewProjectionMatrix);
            projector.projectorMultiplyShader->uniform("u_projector", projectorViewProj);
            projector.projectorMultiplyShader->uniform("u_modelMatrix", terrainModelMatrix);
            projector.projectorMultiplyShader->uniform("u_modelMatrixIT", inv(transpose(terrainModelMatrix)));
            projector.projectorMultiplyShader->texture("s_cookieTex", 0, *projector.cookieTexture, GL_TEXTURE_2D);
            projector.projectorMultiplyShader->texture("s_gradientTex", 1, *projector.gradientTexture, GL_TEXTURE_2D);

            terrainMesh.draw_elements();

            projector.projectorMultiplyShader->unbind();

            glDisable(GL_POLYGON_OFFSET_FILL);
        }

        glDisable(GL_BLEND);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    gpuTimer.stop();

    igm->begin_frame();

    ImGui::Text("Render Time %f ms", gpuTimer.elapsed_ms());
    ImGui::Checkbox("Render Color", &renderColor);
    ImGui::Checkbox("Render Projective", &renderProjective);
    ImGui::ListBox("Src Blendmode", &src_blendmode, listbox_items, IM_ARRAYSIZE(listbox_items), 9);
    ImGui::ListBox("DestBlendmode", &dst_blendmode, listbox_items, IM_ARRAYSIZE(listbox_items), 9);

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
