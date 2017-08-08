#include "index.hpp"
#include <iterator>
#include "svd.hpp"
#include "gl-gizmo.hpp"
#include "octree.hpp"

constexpr const char basic_wireframe_vert[] = R"(#version 330
    layout(location = 0) in vec3 vertex;
    layout(location = 2) in vec3 inColor;
    uniform mat4 u_mvp;
    out vec3 color;
    void main()
    {
        gl_Position = u_mvp * vec4(vertex.xyz, 1);
        color = inColor;
    }
)";

constexpr const char basic_wireframe_frag[] = R"(#version 330
    in vec3 color;
    out vec4 f_color;
    uniform vec3 u_color;
    void main()
    {
        f_color = vec4(u_color, 1);
    }
)";

struct DebugSphere
{
    Pose p;
    float radius;

    Bounds3D get_bounds() const
    {
        const float3 rad3 = float3(radius, radius, radius);
        return { p.transform_coord(-rad3), p.transform_coord(rad3) };
    }
};

bool toggleDebug = false;

struct ExperimentalApp : public GLFWApp
{
    std::unique_ptr<GlShader> wireframeShader;

    GlCamera debugCamera;
    FlyCameraController cameraController;

    UniformRandomGenerator rand;

    std::vector<DebugSphere> meshes;

    GlMesh sphere, box;

    SceneOctree<DebugSphere> octree{ 8,{ { -24, -24, -24 },{ +24, +24, +24 } } };

    std::unique_ptr<GlGizmo> gizmo;
    tinygizmo::rigid_transform xform;

    ExperimentalApp() : GLFWApp(1280, 800, "Octree / Frustum Culling Test App")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        gl_check_error(__FILE__, __LINE__);

        gizmo.reset(new GlGizmo());
        xform.position = { 0.1f, 0.1f, 0.1f };

        wireframeShader.reset(new GlShader(basic_wireframe_vert, basic_wireframe_frag));

        debugCamera.look_at({0, 3.0, -3.5}, {0, 2.0, 0});
        cameraController.set_camera(&debugCamera);

        sphere = make_sphere_mesh(1.f);
        box = make_cube_mesh();
        box.set_non_indexed(GL_LINES);

        for (int i = 0; i < 512; ++i)
        {
            const float3 position = { rand.random_float(48.f) - 24.f, rand.random_float(48.f) - 24.f, rand.random_float(48.f) - 24.f };
            const float radius = rand.random_float(0.125f);

            DebugSphere s; 
            s.p = Pose(float4(0, 0, 0, 1), position);
            s.radius = radius;

            meshes.push_back(std::move(s));
        }

        {
            scoped_timer create("octree create");
            for (auto & sph : meshes)
            {
                octree.create(std::move(SceneNodeContainer<DebugSphere>(sph, sph.get_bounds())));
            }
        }
    }
    
    void on_window_resize(int2 size) override
    {

    }
    
    void on_input(const InputEvent & event) override
    {
        cameraController.handle_input(event);
        if (gizmo) gizmo->handle_input(event);

        if (event.type == InputEvent::KEY && event.value[0] == GLFW_KEY_SPACE && event.action == GLFW_RELEASE)
        {
            toggleDebug = !toggleDebug;
        }
    }
    
    void on_update(const UpdateEvent & e) override
    {
        cameraController.update(e.timestep_ms);
    }
    
    void on_draw() override
    {
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);
        
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);

        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
     
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);

        if (gizmo) gizmo->update(debugCamera, float2(width, height));
        tinygizmo::transform_gizmo("destination", gizmo->gizmo_ctx, xform);

        const float4x4 projectionMatrix = debugCamera.get_projection_matrix((float) width / (float) height);
        const float4x4 viewMatrix = debugCamera.get_view_matrix();
        const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);

        if (toggleDebug)
        {
            octree_debug_draw<DebugSphere>(octree, wireframeShader.get(), &box, &sphere, viewProjectionMatrix, nullptr, float3());
        }

        float3 xformPosition = { xform.position.x, xform.position.y, xform.position.z };

        Frustum camFrustum(viewProjectionMatrix);

        wireframeShader->bind();

        for (auto & sph : meshes)
        {
            const auto sphereModel = mul(sph.p.matrix(), make_scaling_matrix(sph.radius));
            wireframeShader->uniform("u_color", camFrustum.contains(sph.p.position) ? float3(1, 1, 1) : float3(0, 0, 0));
            wireframeShader->uniform("u_mvp", mul(viewProjectionMatrix, sphereModel));
            sphere.draw_elements();
        }

        wireframeShader->unbind();

        std::vector<SceneOctree<DebugSphere>::Octant<DebugSphere> *> visibleNodes;
        {
            scoped_timer t("octree cull");
            octree.cull(camFrustum, visibleNodes, nullptr, false);
        }

        uint32_t visibleObjects = 0;

        for (auto node : visibleNodes)
        {
            float4x4 boxModel = mul(make_translation_matrix(node->box.center()), make_scaling_matrix(node->box.size() / 2.f));
            wireframeShader->bind();
            wireframeShader->uniform("u_mvp", mul(viewProjectionMatrix, boxModel));
            box.draw_elements();

            for (auto obj : node->objects)
            {
                const auto & object = obj.object;
                const auto sphereModel = mul(object.p.matrix(), make_scaling_matrix(object.radius));
                wireframeShader->uniform("u_mvp", mul(viewProjectionMatrix, sphereModel));
                sphere.draw_elements();
            }

            visibleObjects += node->objects.size();

            wireframeShader->unbind();
        }
 
        std::cout << "Visible Objects: " << visibleObjects << std::endl; 

        if (gizmo) gizmo->draw();

        gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
    }
    
};