#include "index.hpp"
#include "vr_hmd.hpp"
#include "vr_renderer.hpp"
#include "static_mesh.hpp"
#include "bullet_debug.hpp"
#include "radix_sort.hpp"
#include "procedural_mesh.hpp"
#include "parabolic_pointer.hpp"
#include <future>
#include "quick_hull.hpp"
#include "algo_misc.hpp"
#include "avl_imgui.hpp"
#include "assets.hpp"

using namespace avl;

struct SceneOctree
{

    struct Node
    {

    };

    Node * root;
    uint32_t depth;

    void add()
    {

    }
    
    void update()
    {

    }

    void remove()
    {

    }
};

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
    Geometry navMesh;

    ParabolicPointerParams params;
    bool regeneratePointer = false;

    std::unique_ptr<MotionControllerVR> leftController;
    std::unique_ptr<MotionControllerVR> rightController;
    std::vector<StaticMesh> controllers;
    StaticMesh teleportationArc;
    bool needsTeleport{ false };
    float3 teleportLocation;

    std::vector<std::shared_ptr<BulletObjectVR>> physicsObjects;

    std::vector<StaticMesh> models;

    uniforms::directional_light directionalLight;
    std::vector<uniforms::point_light> pointLights;

    std::map<std::string, std::shared_ptr<Material>> namedMaterialList;

    void gather(std::vector<Renderable *> & objects, LightCollection & lights)
    {
        uint32_t invalidBounds = 0;
        auto valid_bounds = [&invalidBounds](const Renderable * r) -> bool
        {
            auto result = r->get_bounds().volume() > 0.f ? true : false;
            invalidBounds += (uint32_t) !result;
            return result;
        };

        for (auto & model : models) if (valid_bounds(&model)) objects.push_back(&model);
        for (auto & ctrlr : controllers) if (valid_bounds(&ctrlr)) objects.push_back(&ctrlr);
        if (valid_bounds(&teleportationArc)) objects.push_back(&teleportationArc);

        lights.directionalLight = &directionalLight;
        for (auto & ptLight : pointLights)
        {
            lights.pointLights.push_back(&ptLight);
        }

        // Validate existance of material on object
        for (auto obj : objects)
        {
            Material * mat = obj->get_material();
            if (!mat) throw std::runtime_error("object does not have material");
        }

    }
};

struct VirtualRealityApp : public GLFWApp
{
    uint64_t frameCount = 0;
    bool shouldTakeScreenshot = false;

    AssetDatabase<GlTexture2D> texDatabase;

    std::unique_ptr<VR_Renderer> renderer;
    std::unique_ptr<OpenVR_HMD> hmd;
    ScreenSpaceAutoLayout uiSurface;

    std::vector<std::shared_ptr<GLTextureView3D>> csmViews;

    GlCamera debugCam;
    FlyCameraController cameraController;

    ShaderMonitor shaderMonitor = { "../assets/" };
    
    std::vector<ScreenViewport> viewports;
    Scene scene;

    SimpleTimer t;
    GlGpuTimer gpuTimer;

    std::shared_ptr<BulletEngineVR> physicsEngine;
    std::unique_ptr<PhysicsDebugRenderer> physicsDebugRenderer;

    std::unique_ptr<gui::ImGuiManager> igm;

    VirtualRealityApp();
    ~VirtualRealityApp();

    void setup_physics();

    void setup_scene();

    void on_window_resize(int2 size) override;

    void on_input(const InputEvent & event) override;

    void on_update(const UpdateEvent & e) override;

    void on_draw() override;
};