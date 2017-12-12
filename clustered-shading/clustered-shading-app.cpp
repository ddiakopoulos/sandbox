#include "index.hpp"
#include "imgui/imgui_internal.h"
#include "clustered-shading-app.hpp"

using namespace avl;

constexpr const char default_color_vert[] = R"(#version 330
    layout(location = 0) in vec3 vertex;
    uniform mat4 u_mvp;
    void main()
    {
        gl_Position = u_mvp * vec4(vertex.xyz, 1);
    }
)";

constexpr const char default_color_frag[] = R"(#version 330
    out vec4 f_color;
    uniform vec4 u_color;
    void main()
    {
        f_color = vec4(u_color);
    }
)";

std::unique_ptr<ClusteredShading> clusteredLighting;
static bool animateLights = false;
static int numLights = 256;

shader_workbench::shader_workbench() : GLFWApp(1200, 800, "Clustered Shading Example")
{
    igm.reset(new gui::ImGuiInstance(window));

    gizmo.reset(new GlGizmo());
    xform.position = { 0.0f, 1.f, 0.0f };

    shaderMonitor.watch("../assets/shaders/wireframe_vert.glsl", "../assets/shaders/wireframe_frag.glsl", "../assets/shaders/wireframe_geom.glsl", [&](GlShader & shader)
    {
        wireframeShader = std::move(shader);
    });

    shaderMonitor.watch("../assets/shaders/prototype/simple_clustered_vert.glsl", "../assets/shaders/prototype/simple_clustered_frag.glsl", [&](GlShader & shader)
    {
        clusteredShader = std::move(shader);
    });

    grid.reset(new RenderableGrid(1.0f, 128, 128));

    basicShader = GlShader(default_color_vert, default_color_frag);

    sphereMesh = make_mesh_from_geometry(make_sphere(1.0f));
    //floor = make_cube_mesh();
    floor = make_plane_mesh(48, 48, 1024, 1024);
    angle.resize(256);

    /*
    auto knot = load_geometry_from_ply("../assets/models/geometry/TorusKnotUniform.ply");
    rescale_geometry(knot, 1.f);
    torusKnot = make_mesh_from_geometry(knot);
    */

    for (int i = 0; i < 128; i++)
    {
        randomPositions.push_back({ rand.random_float(-24, 24), rand.random_float(1, 1), rand.random_float(-24, 24), rand.random_float(1, 2) });
    }

    regenerate_lights(numLights);

    debugCamera.nearclip = 0.5f;
    debugCamera.farclip = 64.f;
    debugCamera.look_at({ 0, 3.0, -3.5 }, { 0, 2.0, 0 });
    cameraController.set_camera(&debugCamera);
    cameraController.enableSpring = false;
    cameraController.movementSpeed = 0.25f;

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    clusteredLighting.reset(new ClusteredShading(debugCamera.vfov, float(width) / float(height), debugCamera.nearclip, debugCamera.farclip));
}

shader_workbench::~shader_workbench() { }

void shader_workbench::on_window_resize(int2 size) { }

void shader_workbench::on_input(const InputEvent & event)
{
    cameraController.handle_input(event);
    if (igm) igm->update_input(event);
    if (gizmo) gizmo->handle_input(event);
}

void shader_workbench::on_update(const UpdateEvent & e)
{
    cameraController.update(e.timestep_ms);
    shaderMonitor.handle_recompile();

    if (animateLights)
    {
        for (int i = 0; i < lights.size(); ++i)
        {
            angle[i] += rand.random_float(0.005f, 0.01f);
        }

        for (int i = 0; i < lights.size(); ++i)
        {
            auto & l = lights[i];
            l.positionRadius.x += cos(angle[i] * l.positionRadius.w) * 0.25;
            l.positionRadius.z += sin(angle[i] * l.positionRadius.w) * 0.25;
        }
    }
}

void shader_workbench::regenerate_lights(size_t numLights)
{
    lights.clear();
    float h = 1.f / (float) numLights;
    float val = 0.f;
    for (int i = 0; i < numLights; i++)
    {
        val += h;
        float4 randomPosition = float4(rand.random_float(-10, 10), rand.random_float(0.1, 0.5), rand.random_float(-10, 10), rand.random_float(0.5, 8)); // position + radius
        float3 r3 = float3({ rand.random_float(1), rand.random_float(1), rand.random_float(1) });
        float4 randomColor = float4(r3, 1.f);
        lights.push_back({ randomPosition, randomColor });
    }
}

void shader_workbench::on_draw()
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (igm) igm->begin_frame();

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(0.2f, 0.2f, 0.2f, 0.0f);

    if (gizmo) gizmo->update(debugCamera, float2(width, height));
    tinygizmo::transform_gizmo("frustum", gizmo->gizmo_ctx, xform);

    const float windowAspectRatio = (float)width / (float)height;
    const float4x4 projectionMatrix = debugCamera.get_projection_matrix(windowAspectRatio);
    const float4x4 viewMatrix = debugCamera.get_view_matrix();
    const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);

    glViewport(0, 0, width, height);

    // Cluster Debugging
    /*
    {
        float4x4 debugViewMatrix = inverse(mul(make_translation_matrix({ xform.position.x, xform.position.y, xform.position.z }), make_scaling_matrix({ 1, 1, 1 })));
        float4x4 debugProjectionMatrix = projectionMatrix;

        // Main Camera View
        draw_debug_frustum(&basicShader, mul(debugProjectionMatrix, debugViewMatrix), mul(projectionMatrix, viewMatrix), float4(1, 0, 0, 1));

        auto froxelList = build_debug_froxel_array(&clusteredLighting, debugViewMatrix);
        for (int f = 0; f < froxelList.size(); f++)
        {
            float4 color = float4(1, 1, 1, .1f);

            Frustum frox = froxelList[f];
            if (clusteredLighting->clusterTable[f].lightCount > 0) color = float4(0.25, 0.35, .66, 1);
            draw_debug_frustum(&basicShader, frox, mul(projectionMatrix, viewMatrix), color);
        }
    }
    */

    // Primary scene rendering
    {
        renderTimer.start();

        {
            clusteredShader.bind();

            clusterCPUTimer.start();
            clusteredLighting->cull_lights(viewMatrix, projectionMatrix, lights);
            clusteredLighting->upload(lights);
            clusterCPUTimer.pause();

            clusteredShader.texture("s_clusterTexture", 0, clusteredLighting->clusterTexture, GL_TEXTURE_3D);
            clusteredShader.texture("s_lightIndexTexture", 1, clusteredLighting->lightIndexTexture, GL_TEXTURE_BUFFER);

            clusteredShader.uniform("u_eye", debugCamera.get_eye_point());
            clusteredShader.uniform("u_viewMat", viewMatrix); 
            clusteredShader.uniform("u_viewProj", viewProjectionMatrix);
            clusteredShader.uniform("u_diffuse", float3(1.0f, 1.0f, 1.0f));

            clusteredShader.uniform("u_nearClip", debugCamera.nearclip);
            clusteredShader.uniform("u_farClip", debugCamera.farclip);
            clusteredShader.uniform("u_rcpViewportSize", float2(1.f / (float) width, 1.f / (float) height));

            {
                //float4x4 floorModel = make_scaling_matrix(float3(80, 0.1, 80));
                //floorModel = mul(make_translation_matrix(float3(0, -0.1, 0)), floorModel);
                float4x4 floorModel = make_rotation_matrix({ 1, 0, 0 }, ANVIL_PI / 2.f);
                clusteredShader.uniform("u_modelMatrix", floorModel);
                clusteredShader.uniform("u_modelMatrixIT", inverse(transpose(floorModel)));
                floor.draw_elements();
            }

            /*
            {
                for (int i = 0; i < 48; i++)
                {
                    auto modelMat = mul(make_translation_matrix(randomPositions[i].xyz()), make_scaling_matrix(randomPositions[i].w));
                    clusteredShader.uniform("u_modelMatrix", modelMat);
                    clusteredShader.uniform("u_modelMatrixIT", inverse(transpose(modelMat)));
                    //torusKnot.draw_elements();
                }
            }
            */

            clusteredShader.unbind();
        }

        renderTimer.stop();

        // Visualize the lights
        glDisable(GL_CULL_FACE);
        wireframeShader.bind();
        wireframeShader.uniform("u_eyePos", debugCamera.get_eye_point());
        wireframeShader.uniform("u_viewProjMatrix", viewProjectionMatrix);
        for (auto & l : lights)
        {
            auto translation = make_translation_matrix(l.positionRadius.xyz());
            auto scale = make_scaling_matrix(l.positionRadius.w);
            auto model = mul(translation, scale);
            wireframeShader.uniform("u_modelMatrix", model);
            sphereMesh.draw_elements();
        }
        glEnable(GL_CULL_FACE);
        wireframeShader.unbind();

        //grid->draw(viewProjectionMatrix);
    }
    
    if (gizmo) gizmo->draw();

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::Text("Render Time GPU %f ms", renderTimer.elapsed_ms());
    ImGui::Checkbox("Animate Lights", &animateLights);
    if (ImGui::SliderInt("Num Lights", &numLights, 1, 256)) regenerate_lights(numLights);

    if (igm) igm->end_frame();
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
    catch (...)
    {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
