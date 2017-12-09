#include "index.hpp"
#include "geometric-decals.hpp"

using namespace avl;

shader_workbench::shader_workbench() : GLFWApp(1200, 800, "Geometric Decals")
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    igm.reset(new gui::ImGuiInstance(window));
    gui::make_light_theme();

    shaderMonitor.watch("../assets/shaders/prototype/simple_vert.glsl", "../assets/shaders/prototype/simple_frag.glsl", [&](GlShader & shader)
    {
        litShader = std::move(shader);
    });

    decalTex = load_image("../assets/images/polygon_heart.png");

    std::vector<uint8_t> pixel = { 255, 255, 255, 255 };
    emptyTex.setup(1, 1, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());

    Geometry torusGeom = make_torus();
    GlMesh torusMesh = make_mesh_from_geometry(torusGeom);

    //create_handle_for_asset("default", std::move(torusMesh));

    create_handle_for_asset("torus-geom", std::move(torusGeom));
    //create_handle_for_asset("torus-mesh", std::move(torusMesh));

    GeometryHandle blah = {};
    std::cout << "Blah: " << blah.name << std::endl;
    std::cout << &blah.get() << std::endl;
    std::cout << blah.assigned() << std::endl;

    create_handle_for_asset("torus-mesh", std::move(torusMesh));

    for (auto w : GeometryHandle::list())
    {
        std::cout << "Entry: " << w.name << std::endl;
    }

    StaticMesh m;
    m.geom = "torus-geom";
    m.mesh = "torus-mesh";
    meshes.push_back(std::move(m));

    gizmo.reset(new GlGizmo());

    cam.look_at({ 0, 2.f, 2.f }, { 0, 0.0f, -.1f });
    flycam.set_camera(&cam);
}

shader_workbench::~shader_workbench() { }

void shader_workbench::on_window_resize(int2 size) { }

void shader_workbench::on_input(const InputEvent & event)
{
    igm->update_input(event);
    flycam.handle_input(event);

    if (event.type == InputEvent::KEY)
    {
        if (event.value[0] == GLFW_KEY_SPACE && event.action == GLFW_RELEASE) decals.clear();
        if (event.value[0] == GLFW_KEY_1 && event.action == GLFW_RELEASE) projType = PROJECTION_TYPE_CAMERA;
        if (event.value[0] == GLFW_KEY_2 && event.action == GLFW_RELEASE) projType = PROJECTION_TYPE_NORMAL;
        if (event.value[0] == GLFW_KEY_ESCAPE && event.action == GLFW_RELEASE) exit();
    }

    if (event.type == InputEvent::MOUSE && event.action == GLFW_PRESS)
    {
        if (event.value[0] == GLFW_MOUSE_BUTTON_LEFT)
        {
            for (auto & model : meshes)
            {
                auto worldRay = cam.get_world_ray(event.cursor, float2(event.windowSize));

                RaycastResult rc = model.raycast(worldRay);

                if (rc.hit)
                {
                    float3 position = worldRay.calculate_position(rc.distance);
                    float3 target = (rc.normal * float3(10, 10, 10)) + position;

                    Pose box;

                    // Option A: Camera to mesh (orientation artifacts, better uv projection across hard surfaces)
                    if (projType == PROJECTION_TYPE_CAMERA)
                    {
                        box = Pose(cam.get_pose().orientation, position);
                    }

                    // Option B: Normal to mesh (uv issues)
                    else if (projType == PROJECTION_TYPE_NORMAL)
                    {
                        box = look_at_pose_rh(position, target);
                    }

                    Geometry newDecalGeometry = make_decal_geometry(model.geom.get(), {}, box, float3(0.5f));
                    decals.push_back(make_mesh_from_geometry(newDecalGeometry));
                }
            }

        }
    }

    if (gizmo) gizmo->handle_input(event);
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

    igm->begin_frame();

    if (gizmo) gizmo->update(cam, float2(width, height));

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(0.80f, 0.80f, 0.80f, 1.0f);

    const float4x4 projectionMatrix = cam.get_projection_matrix(float(width) / float(height));
    const float4x4 viewMatrix = cam.get_view_matrix();
    const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);

    auto draw_scene = [this](const float3 & eye, const float4x4 & viewProjectionMatrix)
    {
        litShader.bind();

        litShader.uniform("u_viewProj", viewProjectionMatrix);
        litShader.uniform("u_eye", eye);

        litShader.uniform("u_emissive", float3(0.f, 0.0f, 0.0f));
        litShader.uniform("u_diffuse", float3(0.7f, 0.4f, 0.7f));

        litShader.uniform("u_lights[0].position", float3(+5, 5, 0));
        litShader.uniform("u_lights[0].color", float3(249.f / 255.f, 228.f / 255.f, 157.f / 255.f));
        litShader.uniform("u_lights[1].position", float3(-5, 5, 0));
        litShader.uniform("u_lights[1].color", float3(255.f / 255.f, 242.f / 255.f, 254.f / 255.f));

        for (auto & m : meshes)
        {
            litShader.uniform("u_modelMatrix", m.get_pose().matrix());
            litShader.uniform("u_modelMatrixIT", inverse(transpose(m.get_pose().matrix())));
            litShader.texture("s_diffuseTex", 0, emptyTex, GL_TEXTURE_2D);
            m.draw();
        }

        {
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(-1.0, 1.0);

            for (const auto & decal : decals)
            {
                litShader.uniform("u_modelMatrix", Identity4x4);
                litShader.uniform("u_modelMatrixIT", Identity4x4);
                litShader.texture("s_diffuseTex", 0, decalTex, GL_TEXTURE_2D);
                decal.draw_elements();
            }

            glDisable(GL_POLYGON_OFFSET_FILL);
        }

        litShader.unbind();
    };

    draw_scene(cam.get_eye_point(), viewProjectionMatrix);

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
    catch (...)
    {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
