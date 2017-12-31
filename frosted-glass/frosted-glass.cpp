#include "index.hpp"
#include "frosted-glass.hpp"

using namespace avl;

/*
    Glasses render very nicely in DOOM especially frosted or dirty glasses: decals are used to affect just some part of the glass 
    to make its refraction more or less blurry. The pixel shader computes the refraction “blurriness” factor and selects from the
    blur chain the 2 maps closest to this blurriness factor. It reads from these 2 maps and then linearly interpolates between the 
    2 values to approximate the final blurry color the refraction is supposed to have. This is thanks to this process that glasses
    can produce nice refraction at different levels of blur on a per-pixel-basis.
*/

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

constexpr const char basic_vert[] = R"(#version 450
    layout(location = 0) in vec3 vertex;
    uniform mat4 u_mvp;
    void main()
    {
        gl_Position = u_mvp * vec4(vertex.xyz, 1);
    }
)";

constexpr const char basic_frag[] = R"(#version 450
    out vec4 f_color;
    uniform vec3 u_color;
    void main()
    {
        f_color = vec4(u_color, 1);
    }
)";

constexpr const char basic_textured_vert[] = R"(#version 450
    layout(location = 0) in vec3 vertex;
    layout(location = 3) in vec2 inTexcoord;
    uniform mat4 u_mvp;
    out vec2 v_texcoord;
    void main()
    {
        gl_Position = u_mvp * vec4(vertex.xyz, 1);
        v_texcoord = inTexcoord;
    }
)";

constexpr const char basic_textured_frag[] = R"(#version 450
    in vec2 v_texcoord;
    out vec4 f_color;
    uniform sampler2D s_texture;
    void main()
    {
        vec4 t = texture(s_texture, v_texcoord);
        f_color = vec4(t.xyz, 1);
    }
)";

shader_workbench::shader_workbench() : GLFWApp(1200, 800, "Doom 2k16 Frosted Glass")
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    igm.reset(new gui::ImGuiInstance(window));
    gui::make_light_theme();

    basicShader = GlShader(basic_vert, basic_frag);

    /*
    shaderMonitor.watch("../assets/shaders/prototype/glass_vert.glsl", "../assets/shaders/prototype/glass_frag.glsl", [&](GlShader & shader)
    {
        glassShader = std::move(shader);
    });
    */

    shaderMonitor.watch("../assets/shaders/billboard_vert.glsl", "../assets/shaders/billboard_frag.glsl", [&](GlShader & shader)
    {
        glassShader = std::move(shader);
    });


    glassNormal = load_image("../assets/textures/normal/glass2.png", true);

    cubeTex = load_image("../assets/textures/uv_checker_map/uvcheckermap_01.png", true);

    sceneColor.setup(width, height, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    sceneDepth.setup(width, height, GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glNamedFramebufferTexture2DEXT(sceneFramebuffer, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneColor, 0);
    glNamedFramebufferTexture2DEXT(sceneFramebuffer, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, sceneDepth, 0);
    sceneFramebuffer.check_complete();

    post.reset(new post_chain(float2(width, height)));

    glassSurface = make_plane_mesh(3, 3, 8, 8, false);
    cube = make_cube_mesh();

    floorMesh = make_plane_mesh(12, 12, 8, 8, false);
    floorTex = load_image("../assets/textures/uv_checker_map/uvcheckermap_02.png", false);

    skyMesh = make_sphere_mesh(1.0f);
    skyShader = GlShader(skybox_vert, skybox_frag);

    texturedShader = GlShader(basic_textured_vert, basic_textured_frag);

    // Setup Debug visualizations
    uiSurface.bounds = { 0, 0, (float)width, (float)height };
    uiSurface.add_child({ { 0.0000f, +20 },{ 0, +20 },{ 0.1667f, -10 },{ 0.133f, +10 } });
    uiSurface.add_child({ { 0.1667f, +20 },{ 0, +20 },{ 0.3334f, -10 },{ 0.133f, +10 } });
    uiSurface.add_child({ { 0.3334f, +20 },{ 0, +20 },{ 0.5009f, -10 },{ 0.133f, +10 } });
    uiSurface.add_child({ { 0.5000f, +20 },{ 0, +20 },{ 0.6668f, -10 },{ 0.133f, +10 } });
    uiSurface.layout();

    for (int i = 0; i < 4; ++i)
    {
        auto view = std::make_shared<GLTextureView>(true);
        views.push_back(view);
    }
    gizmo.reset(new GlGizmo());

    cam.look_at({ 0, 9.5f, -6.0f }, { 0, 0.1f, 0 });
    flycam.set_camera(&cam);
}

shader_workbench::~shader_workbench() { }

void shader_workbench::on_window_resize(int2 size) 
{ 
    uiSurface.bounds = { 0, 0, (float)size.x, (float)size.y };
    uiSurface.layout();
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

    auto render_scene = [&](const float3 eye, const float4x4 & viewProj)
    {
        {
            // Largest non-clipped sphere
            float4x4 world = mul(make_translation_matrix(eye), scaling_matrix(float3(cam.farclip * .99f)));

            skyShader.bind();
            float4x4 mvp = mul(viewProj, world);
            skyShader.uniform("u_viewProj", viewProj);
            skyShader.uniform("u_modelMatrix", world);
            skyShader.uniform("u_bottomColor", float3(52.0f / 255.f, 62.0f / 255.f, 82.0f / 255.f));
            skyShader.uniform("u_topColor", float3(81.0f / 255.f, 101.0f / 255.f, 142.0f / 255.f));
            skyMesh.draw_elements();
            skyShader.unbind();

            texturedShader.bind();
            float4x4 cubeModel = make_translation_matrix({ 0, 0, -3 });
            texturedShader.uniform("u_mvp", mul(viewProjectionMatrix, cubeModel));
            texturedShader.texture("s_texture", 0, cubeTex, GL_TEXTURE_2D);
            cube.draw_elements();
            texturedShader.unbind();

            texturedShader.bind();
            float4x4 floorModel = mul(make_translation_matrix({ 0, -2, 0 }), make_rotation_matrix({ 1, 0, 0 }, ANVIL_PI / 2.f));
            texturedShader.uniform("u_mvp", mul(viewProjectionMatrix, floorModel));
            texturedShader.texture("s_texture", 0, floorTex, GL_TEXTURE_2D);
            floorMesh.draw_elements();
            texturedShader.unbind();
        }
    };
    // Main Scene
    {
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glViewport(0, 0, width, height);
        glClearColor(0.6f, 0.6f, 0.6f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        {
            glBindFramebuffer(GL_FRAMEBUFFER, sceneFramebuffer);
            glViewport(0, 0, width, height);
            glClearColor(0.6f, 0.6f, 0.6f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            render_scene(cam.get_eye_point(), viewProjectionMatrix);

            post->execute(sceneColor);
        }

        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, width, height);

            render_scene(cam.get_eye_point(), viewProjectionMatrix);

            glassShader.bind();

            float4x4 glassModel = Identity4x4;

            glassShader.uniform("u_eye", cam.get_eye_point());
            glassShader.uniform("u_viewProj", viewProjectionMatrix);
            glassShader.uniform("u_modelMatrix", glassModel);
            glassShader.uniform("u_modelMatrixIT", inverse(transpose(glassModel)));
            glassShader.texture("s_mip1", 0, sceneColor, GL_TEXTURE_2D);
            glassShader.texture("s_mip2", 1, post->levelTex2[0], GL_TEXTURE_2D);
            glassShader.texture("s_mip3", 2, post->levelTex2[1], GL_TEXTURE_2D);
            glassShader.texture("s_mip4", 3, post->levelTex2[2], GL_TEXTURE_2D);
            glassShader.texture("s_mip5", 4, post->levelTex2[3], GL_TEXTURE_2D);

            glassShader.texture("s_frosted", 5, glassNormal, GL_TEXTURE_2D);

            glassSurface.draw_elements();
            glassShader.unbind();
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    gpuTimer.stop();

    igm->begin_frame();

    ImGui::Text("Render Time %f ms", gpuTimer.elapsed_ms());
    ImGui::Checkbox("Show Debug", &showDebug);

    igm->end_frame();
    if (gizmo) gizmo->draw();

    // Debug Views
    if (showDebug)
    {
        glViewport(0, 0, width, height);
        glDisable(GL_DEPTH_TEST);
        views[0]->draw(uiSurface.children[0]->bounds, float2(width, height), post->levelTex2[0]);
        views[1]->draw(uiSurface.children[1]->bounds, float2(width, height), post->levelTex2[1]);
        views[2]->draw(uiSurface.children[2]->bounds, float2(width, height), post->levelTex2[2]);
        views[3]->draw(uiSurface.children[3]->bounds, float2(width, height), post->levelTex2[3]);
        glEnable(GL_DEPTH_TEST);
    }
 
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
