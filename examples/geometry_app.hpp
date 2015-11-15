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

};

struct Renderable : public Object
{
    GlMesh mesh;
    Geometry geom;
    
    Renderable() {}
    
    Renderable(const Geometry & g) : geom(g)
    {
        mesh = make_mesh_from_geometry(g);
        bounds = g.compute_bounds();
        //mesh.set_non_indexed(GL_LINES);
    }
    
    void draw() const { mesh.draw_elements(); };

    bool check_hit(const Ray & worldRay, float * out = nullptr) const
    {
        auto localRay = pose.inverse() * worldRay;
        localRay.origin /= scale;
        localRay.direction /= scale;
        float outT = 0.0f;
        bool hit = intersect_ray_mesh(localRay, geom, &outT);
        if (out) *out = outT;
        return hit;
    }
};

struct LightObject : public Object
{
    float3 color;
};

enum class GizmoMode : int
{
    Translate,
    Rotate,
    Scale
};

struct Raycast
{
    GlCamera & cam; float2 viewport;
    Raycast(GlCamera & camera, float2 viewport) : cam(camera), viewport(viewport) {}
    Ray from(float2 cursor) { return cam.get_world_ray(cursor, viewport); };
};

struct IGizmo
{
    virtual void on_drag(float2 cursor) = 0;
    virtual void on_release() = 0;
    virtual void on_cancel() = 0;
};

struct TranslationDragger : public IGizmo
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
    
    TranslationDragger(Raycast rc, Renderable & object, const float3 & axis, const float2 & cursor) : rc(rc), object(object), axis(qrot(object.pose.orientation, axis)), initialPosition(object.pose.position)
    {
        initialOffset = compute_offset(cursor);
    }
    
    void on_drag(float2 cursor) final
    {
        const float offset = compute_offset(cursor);
        object.pose.position = initialPosition + (axis * (offset - initialOffset));
    }
    
    void on_release() final {}
    void on_cancel() final {}
};

struct ScalingDragger : public IGizmo
{
    Renderable & object;
    Raycast rc;
    
    float3 axis, initialScale, scaleDirection;
    float initialFactor;
    
    float compute_scale(float2 cursor)
    {
        Ray first = {object.pose.position, axis};
        Ray second = rc.from(cursor);
        float3 lineDir = second.origin - first.origin;
        const float e1e2 = dot(first.direction, second.direction);
        const float denom = 1 - e1e2 * e1e2;
        const float distance = (dot(lineDir, first.direction) - dot(lineDir, second.direction) * e1e2) / denom;
        return distance;
    }
    
    ScalingDragger(Raycast rc, Renderable & object, const float3 & axis, const float2 & cursor) : rc(rc), object(object), axis(qrot(object.pose.orientation, axis)), initialScale(object.scale)
    {
        scaleDirection = axis;
        initialFactor = compute_scale(cursor);
    }
    
    void on_drag(float2 cursor) final
    {
        float scale = compute_scale(cursor) / initialFactor;
        object.scale = initialScale + scaleDirection * ((scale - 1) * dot(initialScale, scaleDirection));
    }
    
    void on_release() final {}
    void on_cancel() final {}
};

struct RotationDragger : public IGizmo
{
    Renderable & object;
    Raycast rc;
    
    float3 axis, edge1;
    float4 initialOrientation;
    
    float3 compute_edge(const float2 & cursor)
    {
        auto ray = rc.from(cursor);
        float3 intersectionPt;
        float t = 0.0;
        intersect_ray_plane(ray, Plane(axis, object.pose.position), &intersectionPt, &t);
        return ray.calculate_position(t) - object.pose.position;
    }
    
    RotationDragger(Raycast rc, Renderable & object, const float3 & axis, const float2 & cursor) : rc(rc), object(object), axis(qrot(object.pose.orientation, axis)), initialOrientation(object.pose.orientation)
    {
        edge1 = compute_edge(cursor);
    }
    
    void on_drag(float2 cursor) final
    {
        float3 newEdge = compute_edge(cursor);
        object.pose.orientation = qmul(make_rotation_quat_between_vectors(edge1, newEdge), initialOrientation);
    }
    
    void on_release() final {}
    void on_cancel() final {}
};

struct ExperimentalApp : public GLFWApp
{
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

    GizmoMode gizmoMode = GizmoMode::Translate;
    std::shared_ptr<IGizmo> activeGizmo;
    
    Renderable translationGeometry;
    Renderable scalingGeometry;
    Renderable rotationGeometry;
    
    Geometry concatenate_geometry(const Geometry & a, const Geometry & b)
    {
        Geometry s;
        s.vertices.insert(s.vertices.end(), a.vertices.begin(), a.vertices.end());
        s.vertices.insert(s.vertices.end(), b.vertices.begin(), b.vertices.end());
        s.faces.insert(s.faces.end(), a.faces.begin(), a.faces.end());
        for (auto & f : s.faces) s.faces.push_back({ (int) a.vertices.size() + f.x, (int) a.vertices.size() + f.y, (int) a.vertices.size() + f.z} );
        return s;
    }
    
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
            
            auto endBox = make_cube();
            auto axisBox = make_cube();
            
            for (auto & v : endBox.vertices)
                v = {(v.x + 16) * 0.25f, v.y * 0.25f, v.z * 0.25f};
            
            for (auto & v : axisBox.vertices)
                v = {(v.x * 2) + 2.125f, v.y * 0.125f, v.z * 0.125f};
            
            auto linearTranslationMesh = concatenate_geometry(endBox, axisBox);
            linearTranslationMesh.compute_normals();
            
            translationGeometry = Renderable(linearTranslationMesh);
            scalingGeometry = Renderable(linearTranslationMesh);
        }
        
        {
            auto ringG = make_ring();
            for (auto & v : ringG.vertices)
                v = {v.z, v.x, v.y};
            rotationGeometry = Renderable(ringG);
        }
        
        {
            proceduralModels.resize(2);
            
            proceduralModels[0] = Renderable(make_sphere(1.0));
            proceduralModels[0].pose.position = float3(0, 0, +5);
            
            proceduralModels[1] = Renderable(make_cube());
            proceduralModels[1].pose.position = float3(0, 0, -5);
        }
        
        gfx::gl_check_error(__FILE__, __LINE__);
    }
    
    void on_window_resize(math::int2 size) override
    {

    }
    
    std::shared_ptr<IGizmo> MakeGizmo(Raycast & rc, Renderable & object, const float3 & axis, const float2 & cursor)
    {
        switch (gizmoMode)
        {
            case GizmoMode::Translate: return std::make_shared<TranslationDragger>(rc, *selectedObject, axis, cursor);
            case GizmoMode::Rotate: return std::make_shared<RotationDragger>(rc, *selectedObject, axis, cursor);
            case GizmoMode::Scale: return std::make_shared<ScalingDragger>(rc, *selectedObject, axis, cursor);
        }
    }
    
    Renderable & GetGizmoMesh()
    {
        switch (gizmoMode)
        {
            case GizmoMode::Translate: return translationGeometry;
            case GizmoMode::Rotate: return rotationGeometry;
            case GizmoMode::Scale: return scalingGeometry;
        }
    }
    
    void on_input(const InputEvent & event) override
    {
        
        if (event.type == InputEvent::MOUSE && event.action == GLFW_PRESS)
        {
            if (event.value[0] == GLFW_MOUSE_BUTTON_LEFT)
            {
                int width, height;
                glfwGetWindowSize(window, &width, &height);
                
                auto worldRay = camera.get_world_ray(event.cursor, float2(event.windowSize.x, event.windowSize.y));
                
                // If we've already selected an object, check to see if we can interact with the gizmo
                if (selectedObject)
                {
                    bool hit = false;
                    for (auto axis : {float3(1, 0, 0), float3(0, 1, 0), float3(0, 0, 1)})
                    {
                        // The handle is composed as a piece of geometry on the X axis.
                        auto p = selectedObject->pose * Pose(make_rotation_quat_between_vectors({1,0,0}, axis), {0,0,0});
                        auto localRay = p.inverse() * worldRay;
                        hit = GetGizmoMesh().check_hit(localRay);
                        
                        if (hit)
                        {
                            Raycast raycast(camera, float2(event.windowSize.x, event.windowSize.y));
                            activeGizmo = MakeGizmo(raycast, *selectedObject, axis, event.cursor);
                            break;
        
                        }
                    }
                    if (!hit) activeGizmo.reset();
                }
                
                // We didn't interact with the gizmo, so check if we should select a new object.
                if (!activeGizmo)
                {

                    // Otherwise, select a new object
                    for (auto & model : proceduralModels)
                    {
                        auto worldRay = camera.get_world_ray(event.cursor, float2(width, height));
                        if (model.check_hit(worldRay))
                        {
                            selectedObject = &model;

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
                if (activeGizmo)
                {
                    activeGizmo->on_drag(event.cursor);
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
        
        if (event.type == InputEvent::KEY)
        {
            if (event.value[0] == GLFW_KEY_1 && event.action == GLFW_RELEASE)
            {
                gizmoMode = GizmoMode::Translate;
            }
            else if (event.value[0] == GLFW_KEY_2 && event.action == GLFW_RELEASE)
            {
                gizmoMode = GizmoMode::Rotate;
            }
            else if (event.value[0] == GLFW_KEY_3 && event.action == GLFW_RELEASE)
            {
                gizmoMode = GizmoMode::Scale;
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
            glEnable(GL_POLYGON_OFFSET_LINE);
            glPolygonOffset(-1, -1);
            
            colorShader->bind();
            
            colorShader->uniform("u_viewProj", viewProj);
            
            if (selectedObject)
            {
                for (auto axis : {float3(1, 0, 0), float3(0, 1, 0), float3(0, 0, 1)})
                {
                    auto p = selectedObject->pose * Pose(make_rotation_quat_between_vectors({1,0,0}, axis), {0,0,0});
                    colorShader->uniform("u_modelMatrix", p.matrix());
                    colorShader->uniform("u_modelMatrixIT", inv(transpose(p.matrix())));
                    colorShader->uniform("u_color", axis);
                    GetGizmoMesh().draw();
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
