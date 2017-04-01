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

/*
 * An octree is a tree data structure in which each internal node has exactly 
 * eight children. Octrees are most often used to partition a three 
 * dimensional space by recursively subdividing it into eight octants.
*/

// http://thomasdiewald.com/blog/?p=1488
// https://www.gamedev.net/resources/_/technical/game-programming/introduction-to-octrees-r3529
// https://cs.brown.edu/courses/csci1230/lectures/CS123_17_Acceleration_Data_Structures_11.3.16.pptx.
// http://www.piko3d.net/tutorials/space-partitioning-tutorial-piko3ds-dynamic-octree/

inline bool inside(const Bounds3D & node, const Bounds3D & other)
{
    // Compare centers
    if (linalg::all(greater(other.max(), node.center())) && linalg::all(less(other.min(), node.center()))) return false;

    // Otherwise ensure we shouldn't move to parent
    return linalg::all(less(other.size(), node.size()));
}

struct SceneOctree
{
 
    struct Node
    {
        Node * parent = nullptr;
        Node(Node * parent) : parent(parent) {}
        Bounds3D box;
        VoxelArray<Node *> arr = { {2, 2, 2} };
        uint32_t occupancy{ 0 };

        int3 get_indices(const Bounds3D & other) const
        {  
            const float3 a = other.center();
            const float3 b = box.center();
            int3 index;
            index.x = (a.x > b.x) ? 1 : 0;
            index.y = (a.y > b.y) ? 1 : 0;
            index.z = (a.z > b.z) ? 1 : 0;
            return index;
        }

        void increase_occupancy()
        {
            occupancy++;
            if (parent) parent->occupancy++;
        }

        void decrease_occupancy()
        {
            occupancy--;
            if (parent) parent->occupancy--;
        }
    };

    Node * root;
    uint32_t maxDepth = { 12 };
    DebugLineRenderer & debugRenderer;

    SceneOctree(DebugLineRenderer & debugRenderer) : debugRenderer(debugRenderer)
    {
        root = new Node(nullptr);
        root->box = Bounds3D({ -128, -128, -128 }, { +128, +128, +128 });
    }

    void add(Renderable * node, Node * child, int depth = 0)
    {
        if (!child) child = root;

        Bounds3D bounds = node->get_bounds();

        if (depth < maxDepth)
        {
            int3 lookup = child->get_indices(bounds);

            // No child for this octant
            if (child->arr[lookup] == nullptr)
            {
                child->arr[lookup] = new Node(child);

                const float3 octantMin = child->box.min();
                const float3 octantMax = child->box.max();
                const float3 octantCenter = child->box.center();

                float3 min, max;
                for (int axis : { 0, 1, 2 })
                {
                    if (lookup[axis] == 0)
                    {
                        min[axis] = octantMin[axis];
                        max[axis] = octantCenter[axis];
                    }
                    else
                    {
                        min[axis] = octantCenter[axis];
                        max[axis] = octantMax[axis];
                    }
                }

                child->arr[lookup]->box = Bounds3D(min, max);
            }

            // Recurse into a new depth
            add(node, child->arr[lookup], ++depth);
        }
        else
        {
            child->increase_occupancy();
            // todo: add renderable to list
        }
    }
    
    void create(Renderable * node)
    {
        // todo: valid box / root check
        Bounds3D bounds = node->get_bounds();

        if ()
    }

    void remove(Renderable * node)
    {

    }

    // Debugging Only
    void draw(const float4x4 & viewProj, Node * node)
    {
        if (!node) node = root;
        if (node->occupancy == 0) return;

        debugRenderer.draw_box(node->box);

        // Recurse into children
        Node * child;
        if ((child = node->arr[{0, 0, 0}]) != nullptr) draw(viewProj, child);
        if ((child = node->arr[{0, 0, 1}]) != nullptr) draw(viewProj, child);
        if ((child = node->arr[{0, 1, 0}]) != nullptr) draw(viewProj, child);
        if ((child = node->arr[{0, 1, 1}]) != nullptr) draw(viewProj, child);
        if ((child = node->arr[{1, 0, 0}]) != nullptr) draw(viewProj, child);
        if ((child = node->arr[{1, 0, 1}]) != nullptr) draw(viewProj, child);
        if ((child = node->arr[{1, 1, 0}]) != nullptr) draw(viewProj, child);
        if ((child = node->arr[{1, 1, 1}]) != nullptr) draw(viewProj, child);
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

    std::unique_ptr<SceneOctree> octree;

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