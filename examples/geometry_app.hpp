#include "anvil.hpp"

using namespace math;
using namespace util;
using namespace gfx;

constexpr const char colorVertexShader[] = R"(#version 330
    layout(location = 0) in vec3 vertex;
    uniform mat4 u_modelMatrix;
    uniform mat4 u_viewProj;
    uniform vec3 u_color;
    out vec3 color;
    void main()
    {
        vec4 worldPos = u_modelMatrix * vec4(vertex, 1);
        gl_Position = u_viewProj * worldPos;
        color = u_color;
    }
)";

constexpr const char colorFragmentShader[] = R"(#version 330
    in vec3 color;
    out vec4 f_color;
    void main()
    {
        f_color = vec4(color.rgb, 1);
    }
)";


struct Object
{
    Pose pose;
    float3 scale;
    Object() : scale(1, 1, 1) {}
    float4x4 get_model() const { return mul(pose.matrix(), make_scaling_matrix(scale)); }
    math::Box<float, 3> bounds;
    
    bool check_hit(const Ray & worldRay) const
    {
        auto localRay = pose.inverse() * worldRay;
        localRay.origin /= scale;
        localRay.direction /= scale;
        return intersect_ray_box(localRay, bounds.min, bounds.max);
    }
};

struct Renderable : public Object
{
    GlMesh mesh;
    void draw() const { mesh.draw_elements(); };
    void build(const Geometry & g)
    {
        mesh = make_mesh_from_geometry(g);
        bounds = g.compute_bounds();
    };
};

struct LightObject : public Object
{
    float3 color;
};


struct Raycast
{
    GlCamera & cam; float2 viewport;
    Raycast(GlCamera & camera, float2 viewport) : cam(camera), viewport(viewport) {}
    Ray from(float2 cursor) { return cam.get_world_ray(cursor, viewport); };
};

struct Handle
{
    float3 axis;
    Handle(float3 axis) : axis(axis) {}
};

struct TranslationHandle : public Renderable, public Handle
{
    GlMesh mesh;
    Geometry geom;
    
    TranslationHandle(float3 axis, float size) : Handle(axis)
    {
        geom = make_cube();
        build(geom);
        pose.orientation = {0, 0, 0, 1};
        pose.position = size * axis;
        scale = (size * axis) + float3(0.15, 0.15, 0.15);
    }
};

struct TranslationGizmo
{
    std::vector<std::shared_ptr<TranslationHandle>> handles;
    
    TranslationGizmo()
    {
        handles.emplace_back(new TranslationHandle({1, 0, 0}, 2)); // X
        handles.emplace_back(new TranslationHandle({0, 1, 0}, 2)); // Y
        handles.emplace_back(new TranslationHandle({0, 0, 1}, 2)); // Z
    }
    
    void update_pose(Pose p)
    {
        for (auto h : handles)
            h->pose.position = p.position + (h->scale * h->axis); // Offset for alignment of handle
    }
    
    float3 hit_test_axis(const Ray & worldRay)
    {
        for (auto h : handles)
            if (h->check_hit(worldRay))
                return h->axis;
        return {0, 0, 0};
    }
};

struct TranslationDragger
{
    Renderable & object;
    Raycast rc;
    
    float3 axis;
    float3 initialPosition;
    float initialOffset;
    
    float compute_offset(float2 cursor)
    {
        Ray first = {initialPosition, axis}; // From the object origin along the axis of travel
        Ray second = rc.from(cursor); // world space click
        float3 lineDir = second.origin - first.origin;
        const float e1e2 = dot(first.direction, second.direction);
        const float denom = 1 - e1e2 * e1e2;
        const float distance = (dot(lineDir, first.direction) - dot(lineDir, second.direction) * e1e2) / denom;
        return distance;
    }
    
    TranslationDragger(Raycast rc, Renderable & object, const float3 & axis, const float2 & cursor) : rc(rc), object(object), axis(axis), initialPosition(object.pose.position)
    {
        initialOffset = compute_offset(cursor);
    }
    
    void on_drag(float2 cursor)
    {
        auto offset = compute_offset(cursor);
        object.pose.position = initialPosition + (axis * (offset - initialOffset));
    }
 
};

struct ExperimentalApp : public GLFWApp
{
    TranslationGizmo translationGizmo;
    
    uint64_t frameCount = 0;

    GlCamera camera;
    PreethamProceduralSky preethamSky;
    RenderableGrid grid;
    FPSCameraController cameraController;
    
    std::unique_ptr<GlShader> simpleShader;
    std::unique_ptr<GlShader> colorShader;
    
    std::vector<LightObject> lights;
    
    std::vector<Renderable> proceduralModels;
    std::vector<Renderable> debugModels;

    Renderable * selectedObject = nullptr;
    
    bool isDragging = false;
    
    std::shared_ptr<TranslationDragger> dragger;
    
    ExperimentalApp() : GLFWApp(820, 480, "Geometry App")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        grid = RenderableGrid(1, 100, 100);
        cameraController.set_camera(&camera);
        
        lights.resize(2);
        
        lights[0].color = float3(44.f / 255.f, 168.f / 255.f, 220.f / 255.f);
        lights[0].pose.position = float3(25, 15, 0);
        
        lights[1].color = float3(220.f / 255.f, 44.f / 255.f, 201.f / 255.f);
        lights[1].pose.position = float3(-25, 15, 0);
        
        simpleShader.reset(new gfx::GlShader(read_file_text("assets/shaders/simple_vert.glsl"), read_file_text("assets/shaders/simple_frag.glsl")));
        colorShader.reset(new gfx::GlShader(colorVertexShader, colorFragmentShader));
        
        {
            proceduralModels.resize(11);
            
            proceduralModels[0].build(make_sphere(1.0));
            proceduralModels[0].pose.position = float3(0, 0, +5);
            
            proceduralModels[1].build(make_cube());
            proceduralModels[1].pose.position = float3(0, 0, -5);
        }
        
        gfx::gl_check_error(__FILE__, __LINE__);
    }
    
    void on_window_resize(math::int2 size) override
    {

    }
    
    void on_input(const InputEvent & event) override
    {
        
        if (event.type == InputEvent::MOUSE && event.action == GLFW_PRESS)
        {
            if (event.value[0] == GLFW_MOUSE_BUTTON_LEFT)
            {
                int width, height;
                glfwGetWindowSize(window, &width, &height);
                
                // If we've already selected an object, check to see if we can interact with the gizmo
                if (selectedObject)
                {
                    auto worldRay = camera.get_world_ray(event.cursor, float2(event.windowSize.x, event.windowSize.y));
                    auto axis = translationGizmo.hit_test_axis(worldRay);
                    
                    if (length(axis) > 0)
                    {
                        Raycast raycast(camera, float2(event.windowSize.x, event.windowSize.y));
                        dragger = std::make_shared<TranslationDragger>(raycast, *selectedObject, axis, event.cursor);
                        translationGizmo.update_pose(selectedObject->pose);
                    }
                    else
                    {
                        dragger.reset();
                    }

                }
                
                // We didn't interact with the gizmo, so check if we should select a new object.
                if (!dragger)
                {

                    // Otherwise, select a new object
                    for (auto & model : proceduralModels)
                    {
                        auto worldRay = camera.get_world_ray(event.cursor, float2(width, height));
                        if (model.check_hit(worldRay))
                        {
                            selectedObject = &model;
                            translationGizmo.update_pose(selectedObject->pose);
                            break;
                        }
                        selectedObject = nullptr;
                    }
                }

            }
        }
        
        if (event.type == InputEvent::CURSOR && isDragging)
        {
            if (selectedObject)
            {
                if (dragger)
                {
                    dragger->on_drag(event.cursor);
                    translationGizmo.update_pose(selectedObject->pose);
                }
            }
        }
        
        if (event.type == InputEvent::MOUSE)
        {
            if (event.is_mouse_down())
            {
                isDragging = true;
            }
            
            if (event.is_mouse_up())
            {
                isDragging = false;
            }
        }
        
        cameraController.handle_input(event);
    }
    
    void on_update(const UpdateEvent & e) override
    {
        cameraController.update(e.timestep_ms);
    }
    
    void on_draw() override
    {
        glfwMakeContextCurrent(window);
        
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);

        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
     
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.1f, 0.1f, 0.5f, 1.0f);

        const auto proj = camera.get_projection_matrix((float) width / (float) height);
        const float4x4 view = camera.get_view_matrix();
        const float4x4 viewProj = mul(proj, view);
        
        preethamSky.render(viewProj, camera.get_eye_point(), camera.farClip);
        
        // Simple Shader
        {
            simpleShader->bind();
            
            simpleShader->uniform("u_viewProj", viewProj);
            simpleShader->uniform("u_eye", camera.get_eye_point());
            
            simpleShader->uniform("u_emissive", float3(.10f, 0.10f, 0.10f));
            simpleShader->uniform("u_diffuse", float3(0.4f, 0.4f, 0.25f));
            
            for (int i = 0; i < lights.size(); i++)
            {
                auto light = lights[i];
                
                simpleShader->uniform("u_lights[" + std::to_string(i) + "].position", light.pose.position);
                simpleShader->uniform("u_lights[" + std::to_string(i) + "].color", light.color);
            }

            for (const auto & model : proceduralModels)
            {
                simpleShader->uniform("u_modelMatrix", model.get_model());
                simpleShader->uniform("u_modelMatrixIT", inv(transpose(model.get_model())));
                model.draw();
            }
            

            simpleShader->unbind();
        }
        
        // Color gizmo shader
        {
            colorShader->bind();
            
            colorShader->uniform("u_viewProj", viewProj);
            
            if (selectedObject)
            {
                for (auto handle : translationGizmo.handles)
                {
                    colorShader->uniform("u_modelMatrix", handle->get_model());
                    colorShader->uniform("u_color", handle->axis);
                    handle->draw();
                }
            }
            
            colorShader->unbind();
        }
        
        grid.render(proj, view, {0, -5, 0});

        gfx::gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
        
        frameCount++;
    }
    
};
