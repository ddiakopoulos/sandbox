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

using namespace avl;

/*
float4x4 make_directional_light_view_proj(const uniforms::directional_light & light, const float3 & eyePoint)
{
    const Pose p = look_at_pose(eyePoint, eyePoint + -light.direction);
    const float halfArea = light.amount * 0.5f;
    return mul(make_orthographic_matrix(-halfArea, halfArea, -halfArea, halfArea, -halfArea, halfArea), make_view_matrix_from_pose(p));
}

float4x4 make_spot_light_view_proj(const uniforms::spot_light & light)
{
    const Pose p = look_at_pose(light.position, light.position + light.direction);
    return mul(make_perspective_matrix(to_radians(light.cutoff * 2.0f), 1.0f, 0.1f, 1000.f), make_view_matrix_from_pose(p));
}
*/

class imgui_menu_stack
{
    bool * keys;
    int current_mods;
    std::vector<bool> open;

public:

    imgui_menu_stack(const GLFWApp & app, bool * keys) : current_mods(app.get_mods()), keys(keys) { }

    void app_menu_begin()
    {
        assert(open.empty());
        open.push_back(ImGui::BeginMainMenuBar());
    }

    void begin(const char * label, bool enabled = true)
    {
        open.push_back(open.back() ? ImGui::BeginMenu(label, true) : false);
    }

    bool item(const char * label, int mods = 0, int key = 0, bool enabled = true)
    {
        bool invoked = (key && mods == current_mods && keys[key]);
        if (open.back())
        {
            std::string shortcut;
            if (key)
            {
                if (mods & GLFW_MOD_CONTROL) shortcut += "Ctrl+";
                if (mods & GLFW_MOD_SHIFT) shortcut += "Shift+";
                if (mods & GLFW_MOD_ALT) shortcut += "Alt+";
                if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) shortcut += static_cast<char>('A' + (key - GLFW_KEY_A));
                else if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) shortcut += static_cast<char>('0' + (key - GLFW_KEY_0));
                else if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F25) shortcut += 'F' + std::to_string(1 + (key - GLFW_KEY_F1));
                else if (key == GLFW_KEY_SPACE) shortcut += "Space";
                else if (key == GLFW_KEY_APOSTROPHE) shortcut += '\'';
                else if (key == GLFW_KEY_COMMA) shortcut += ',';
                else if (key == GLFW_KEY_MINUS) shortcut += '-';
                else if (key == GLFW_KEY_PERIOD) shortcut += '.';
                else if (key == GLFW_KEY_SLASH) shortcut += '/';
                else if (key == GLFW_KEY_SEMICOLON) shortcut += ';';
                else if (key == GLFW_KEY_EQUAL) shortcut += '=';
                else if (key == GLFW_KEY_LEFT_BRACKET) shortcut += '[';
                else if (key == GLFW_KEY_BACKSLASH) shortcut += '\\';
                else if (key == GLFW_KEY_RIGHT_BRACKET) shortcut += ']';
                else if (key == GLFW_KEY_GRAVE_ACCENT) shortcut += '`';
                else if (key == GLFW_KEY_ESCAPE) shortcut += "Escape";
                else if (key == GLFW_KEY_ENTER) shortcut += "Enter";
                else if (key == GLFW_KEY_TAB) shortcut += "Tab";
                else if (key == GLFW_KEY_BACKSPACE) shortcut += "Backspace";
                else if (key == GLFW_KEY_INSERT) shortcut += "Insert";
                else if (key == GLFW_KEY_DELETE) shortcut += "Delete";
                else if (key == GLFW_KEY_RIGHT) shortcut += "Right Arrow";
                else if (key == GLFW_KEY_LEFT) shortcut += "Left Arrow";
                else if (key == GLFW_KEY_DOWN) shortcut += "Down Arrow";
                else if (key == GLFW_KEY_UP) shortcut += "Up Arrow";
                else if (key == GLFW_KEY_PAGE_UP) shortcut += "Page Up";
                else if (key == GLFW_KEY_PAGE_DOWN) shortcut += "Page Down";
                else if (key == GLFW_KEY_HOME) shortcut += "Home";
                else if (key == GLFW_KEY_END) shortcut += "End";
                else if (key == GLFW_KEY_CAPS_LOCK) shortcut += "Caps Lock";
                else if (key == GLFW_KEY_SCROLL_LOCK) shortcut += "Scroll Lock";
                else if (key == GLFW_KEY_NUM_LOCK) shortcut += "Num Lock";
                else if (key == GLFW_KEY_PRINT_SCREEN) shortcut += "Print Screen";
                else if (key == GLFW_KEY_PAUSE) shortcut += "Pause";
                else assert(false && "bad shortcut key");
            }
            invoked |= ImGui::MenuItem(label, shortcut.c_str(), false, enabled);
        }
        return invoked;
    }

    void end()
    {
        if (open.back()) ImGui::EndMenu();
        open.pop_back();
    }

    void app_menu_end()
    {
        if (open.back()) ImGui::EndMainMenuBar();
        open.pop_back();
        assert(open.empty());
    }
};

void imgui_fixed_window_begin(const char * name, const ui_rect & r)
{
    ImGui::SetNextWindowPos(r.min);
    ImGui::SetNextWindowSize(r.max - r.min);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0, 0));
    bool result = ImGui::Begin(name, NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextColored({ 1,1,0.5f,1 }, name);
    ImGui::Separator();
    assert(result);
}

void imgui_fixed_window_end()
{
    ImGui::End();
    ImGui::PopStyleVar(2);
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