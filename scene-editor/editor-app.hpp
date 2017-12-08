#include "index.hpp"
#include "gl-gizmo.hpp"
#include "gl-imgui.hpp"

#include "material.hpp"
#include "fwd_renderer.hpp"
#include "uniforms.hpp"
#include "assets.hpp"
#include "scene.hpp"

static inline Pose to_linalg(tinygizmo::rigid_transform & t)
{
    return{ reinterpret_cast<float4 &>(t.orientation), reinterpret_cast<float3 &>(t.position) };
}

static inline tinygizmo::rigid_transform from_linalg(Pose & p)
{
    return{ reinterpret_cast<minalg::float4 &>(p.orientation), reinterpret_cast<minalg::float3 &>(p.position) };
}

struct auto_layout
{

    struct ucoord
    {
        float a, b;
        float resolve(float min, float max) const { return min + a * (max - min) + b; }
    };

    struct urect
    {
        ucoord x0, y0, x1, y1;
        Bounds2D resolve(const Bounds2D & r) const { return{ x0.resolve(r.min().x, r.max().x), y0.resolve(r.min().y, r.max().y), x1.resolve(r.min().x, r.max().x), y1.resolve(r.min().y, r.max().y) }; }
        bool is_fixed_width() const { return x0.a == x1.a; }
        bool is_fixed_height() const { return y0.a == y1.a; }
        float fixed_width() const { return x1.b - x0.b; }
        float fixed_height() const { return y1.b - y0.b; }
    };

    float aspectRatio{ 1 };
    urect placement{ { 0,0 },{ 0,0 },{ 1,0 },{ 1,0 } };
    Bounds2D bounds;

    std::vector<std::shared_ptr<auto_layout>> children;

    void add_child(auto_layout::urect placement, std::shared_ptr<auto_layout> child = std::make_shared<auto_layout>())
    {
        child->placement = placement;
        children.push_back(child);
    }

    void layout()
    {
        for (auto & child : children)
        {
            auto size = child->bounds.size();
            child->bounds = child->placement.resolve(bounds);
            auto childAspect = child->bounds.width() / child->bounds.height();
            if (childAspect > 0)
            {
                float xpadding = (1 - std::min((child->bounds.height() * childAspect) / child->bounds.width(), 1.0f)) / 2;
                float ypadding = (1 - std::min((child->bounds.width() / childAspect) / child->bounds.height(), 1.0f)) / 2;
                child->bounds = urect{ { xpadding, 0 },{ ypadding, 0 },{ 1 - xpadding, 0 },{ 1 - ypadding, 0 } }.resolve(child->bounds);
            }
            if (child->bounds.size() != size) child->layout();
        }
    }
};

template<typename ObjectType>
class editor_controller
{
    GlGizmo gizmo;
    tinygizmo::rigid_transform gizmo_selection;     // Center of mass of multiple objects or the pose of a single object
    tinygizmo::rigid_transform last_gizmo_selection; 

    Pose selection;
    std::vector<ObjectType *> selected_objects;     // Array of selected objects
    std::vector<Pose> relative_transforms;          // Pose of the objects relative to the selection
    
    bool gizmo_active = false;

    void compute_selection()
    {
        // No selected objects? The selection pose is nil
        if (selected_objects.size() == 0)
        {
            selection = {};
        }
        // Single object selection
        else if (selected_objects.size() == 1)
        {
            selection = (*selected_objects.begin())->get_pose();
        }
        // Multi-object selection
        else
        {
            // todo: orientation... bounding boxes?
            float3 center_of_mass = {};
            for (auto obj : selected_objects) center_of_mass += obj->get_pose().position;
            center_of_mass /= float3(selected_objects.size());
            selection.position = center_of_mass;
        }

        compute_relative_transforms();

        gizmo_selection = from_linalg(selection);
    }

    void compute_relative_transforms()
    {
        relative_transforms.clear();

        for (auto s : selected_objects)
        {
            relative_transforms.push_back(selection.inverse() * s->get_pose());
        }
    }

public:

    editor_controller() { }

    bool selected(ObjectType * object) const
    {
        return std::find(selected_objects.begin(), selected_objects.end(), object) != selected_objects.end();
    }

    std::vector<ObjectType *> & get_selection()
    {
        return selected_objects;
    }

    void set_selection(const std::vector<ObjectType *> & new_selection)
    {
        selected_objects = new_selection;
        compute_selection();
    }

    void update_selection(ObjectType * object)
    { 
        auto it = std::find(std::begin(selected_objects), std::end(selected_objects), object);
        if (it == std::end(selected_objects)) selected_objects.push_back(object);
        else selected_objects.erase(it);
        compute_selection();
    }

    void clear()
    {
        selected_objects.clear();
        compute_selection();
    }

    bool active() const
    {
        return gizmo_active;
    }

    void on_input(const InputEvent & event)
    {
        gizmo.handle_input(event);
    }

    void on_update(const GlCamera & camera, const float2 viewport_size)
    {
        gizmo.update(camera, viewport_size);
        gizmo_active = tinygizmo::transform_gizmo("editor-controller", gizmo.gizmo_ctx, gizmo_selection);

        // Perform editing updates on selected objects
        if (gizmo_selection != last_gizmo_selection)
        {
            for (int i = 0; i < selected_objects.size(); ++i)
            {
                ObjectType * object = selected_objects[i];
                auto newPose = to_linalg(gizmo_selection) * relative_transforms[i];
                object->set_pose(newPose);
            }
        }

        last_gizmo_selection = gizmo_selection;
    }

    void on_draw()
    {
        if (selected_objects.size())
        {
            gizmo.draw();
        }
    }
};

struct scene_editor_app final : public GLFWApp
{
    GlCamera cam;
    FlyCameraController flycam;
    ShaderMonitor shaderMonitor { "../assets/" };

    uint32_t pbrProgramAsset = -1;

    Scene scene;

    std::unique_ptr<gui::ImGuiInstance> igm;
    std::unique_ptr<PhysicallyBasedRenderer> renderer;
    std::unique_ptr<editor_controller<GameObject>> editor;

    auto_layout uiSurface;
    std::vector<std::shared_ptr<GLTextureView>> debugViews;

    scene_editor_app();
    ~scene_editor_app();

    void on_window_resize(int2 size) override;
    void on_input(const InputEvent & event) override;
    void on_update(const UpdateEvent & e) override;
    void on_draw() override;
    void on_drop(std::vector <std::string> filepaths) override;
};