#include "index.hpp"
#include "vr_hmd.hpp"
#include "vr_renderer.hpp"
#include "static_mesh.hpp"
#include "bullet_debug.hpp"
#include "radix_sort.hpp"
#include "procedural_mesh.hpp"
#include "sobol_sequence.h"
#include "kmeans.hpp"

using namespace avl;

class DebugLineRenderer : public DebugRenderable
{
    struct Vertex { float3 position; float3 color; };
    std::vector<Vertex> vertices;
    GlMesh debugMesh;
    GlShader debugShader;

    Geometry axis = make_axis();
    Geometry box = make_cube();
    Geometry sphere = make_sphere(1.f);

    constexpr static const char debugVertexShader[] = R"(#version 330 
    layout(location = 0) in vec3 v; 
    layout(location = 1) in vec3 c; 
    uniform mat4 u_mvp; 
    out vec3 oc; 
    void main() { gl_Position = u_mvp * vec4(v.xyz, 1); oc = c; })";

    constexpr static const char debugFragmentShader[] = R"(#version 330 
    in vec3 oc; 
    out vec4 f_color; 
    void main() { f_color = vec4(oc.rgb, 1); })";

public:

    DebugLineRenderer()
    {
        debugShader = GlShader(debugVertexShader, debugFragmentShader);
    }

    virtual void draw(const float4x4 & viewProj) override
    {
        debugMesh.set_vertices(vertices.size(), vertices.data(), GL_DYNAMIC_DRAW);
        debugMesh.set_attribute(0, &Vertex::position);
        debugMesh.set_attribute(1, &Vertex::color);
        debugMesh.set_non_indexed(GL_LINES);

        auto model = Identity4x4;
        auto modelViewProjectionMatrix = mul(viewProj, model);

        debugShader.bind();
        debugShader.uniform("u_mvp", modelViewProjectionMatrix);
        debugMesh.draw_elements();
        debugShader.unbind();
    }

    void clear()
    {
        vertices.clear();
    }

    void draw_line(const float3 & from, const float3 & to, const float3 color = float3(1, 1, 1))
    {
        vertices.push_back({from, color});
        vertices.push_back({to, color});
    }

    void draw_box(const Pose & pose, const float & half, const float3 color = float3(1, 1, 1))
    {
        // todo - apply exents and position
        for (const auto v : box.vertices) vertices.push_back({ v, color });
    }

    void draw_sphere(const Pose & pose, const float & radius, const float3 color = float3(1, 1, 1))
    {
        // todo - apply radius and position
        for (const auto v : sphere.vertices) vertices.push_back({ v, color });
    }

    void draw_axis(const Pose & pose, const float3 color = float3(1, 1, 1))
    {
        for (int i = 0; i < axis.vertices.size(); ++i)
        {
            auto v = axis.vertices[i];
            v = pose.transform_coord(v);
            vertices.push_back({ v, axis.colors[i].xyz() });
        }
    }

    void draw_frustum(const Pose & pose, const float4x4 & view, const float3 color = float3(1, 1, 1))
    {
        // todo - make_near_clip_coords / make_far_clip_coords
    }

};

float4x4 make_directional_light_view_proj(const uniforms::directional_light & light, const float3 & eyePoint)
{
    const Pose p = look_at_pose(eyePoint, eyePoint + -light.direction);
    const float halfSize = light.size * 0.5f;
    return mul(make_orthographic_matrix(-halfSize, halfSize, -halfSize, halfSize, -halfSize, halfSize), make_view_matrix_from_pose(p));
}

float4x4 make_spot_light_view_proj(const uniforms::spot_light & light)
{
    const Pose p = look_at_pose(light.position, light.position + light.direction);
    return mul(make_perspective_matrix(to_radians(light.cutoff * 2.0f), 1.0f, 0.1f, 1000.f), make_view_matrix_from_pose(p));
}

struct ScreenViewport
{
    float2 bmin, bmax;
    GLuint texture;
};

// MotionControllerVR wraps BulletObjectVR and is responsible for creating a controlled physically-activating 
// object, and keeping the physics engine aware of the latest user-controlled pose.
class MotionControllerVR
{
    Pose latestPose;

    void update_physics(const float dt, BulletEngineVR * engine)
    {
        physicsObject->body->clearForces();
        physicsObject->body->setWorldTransform(to_bt(latestPose.matrix()));
    }

public:

    std::shared_ptr<BulletEngineVR> engine;
    const OpenVR_Controller & ctrl;
    std::shared_ptr<OpenVR_Controller::ControllerRenderData> renderData;

    btCollisionShape * controllerShape{ nullptr };
    BulletObjectVR * physicsObject{ nullptr };

    MotionControllerVR(std::shared_ptr<BulletEngineVR> engine, const OpenVR_Controller & ctrl, std::shared_ptr<OpenVR_Controller::ControllerRenderData> renderData)
        : engine(engine), ctrl(ctrl), renderData(renderData)
    {

        // Physics tick
        engine->add_task([=](float time, BulletEngineVR * engine)
        {
            this->update_physics(time, engine);
        });

        controllerShape = new btBoxShape(btVector3(0.096, 0.096, 0.0123)); // fixme to use renderData

        physicsObject = new BulletObjectVR(new btDefaultMotionState(), controllerShape, engine->get_world(), 0.5f); // Controllers require non-zero mass

        physicsObject->body->setFriction(2.f);
        physicsObject->body->setRestitution(0.1f);
        physicsObject->body->setGravity(btVector3(0, 0, 0));
        physicsObject->body->setActivationState(DISABLE_DEACTIVATION);

        engine->add_object(physicsObject);
    }

    ~MotionControllerVR()
    {
        engine->remove_object(physicsObject);
        delete physicsObject;
    }

    void update(const Pose & latestControllerPose)
    {
        latestPose = latestControllerPose;

        auto collisionList = physicsObject->CollideWorld();
        for (auto & c : collisionList)
        {
            // Contact points
        }
    }

};

struct Scene
{
    RenderableGrid grid {0.25f, 24, 24 };

    std::unique_ptr<MotionControllerVR> leftController;
    std::unique_ptr<MotionControllerVR> rightController;

    std::vector<std::shared_ptr<BulletObjectVR>> physicsObjects;

    std::vector<StaticMesh> models;
    std::vector<StaticMesh> controllers;

    std::map<std::string, std::shared_ptr<Material>> namedMaterialList;

    std::vector<Renderable *> gather()
    {
        std::vector<Renderable *> objectList;
        for (auto & model : models) objectList.push_back(&model);
        for (auto & ctrlr : controllers) objectList.push_back(&ctrlr);
        return objectList;
    }
};

struct VirtualRealityApp : public GLFWApp
{
    std::unique_ptr<VR_Renderer> renderer;
    std::unique_ptr<OpenVR_HMD> hmd;

    GlCamera debugCam;
    FlyCameraController cameraController;

    ShaderMonitor shaderMonitor = { "../assets/" };
    
    std::vector<ScreenViewport> viewports;
    Scene scene;

    std::shared_ptr<BulletEngineVR> physicsEngine;
    std::unique_ptr<PhysicsDebugRenderer> physicsDebugRenderer;

    DebugLineRenderer sceneDebugRenderer;

    VirtualRealityApp();
    ~VirtualRealityApp();

    void setup_physics();

    void setup_scene();

    void on_window_resize(int2 size) override;

    void on_input(const InputEvent & event) override;

    void on_update(const UpdateEvent & e) override;

    void on_draw() override;
};