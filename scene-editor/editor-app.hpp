#include "index.hpp"
#include "gl-gizmo.hpp"

inline Pose to_linalg(tinygizmo::rigid_transform & t)
{
    return{ reinterpret_cast<float4 &>(t.orientation), reinterpret_cast<float3 &>(t.position) };
}

template<typename ObjectType>
class editor_controller
{
    GlGizmo gizmo;
    tinygizmo::rigid_transform gizmo_selection;     // Center of mass of multiple objects or the pose of a single object

    Pose selection;
    std::vector<ObjectType *> selected_objects;         // Array of selected objects
    std::vector<Pose> relative_transforms;          // Pose of the objects relative to the selection

    void compute_selection()
    {
        if (selected_objects.size() == 0) selection = {};
        else if (selected_objects.size() == 1) selection = (*selected_objects.begin())->pose;
        else
        {
            // todo: orientation... bounding boxes?
            float3 center_of_mass = {};
            for (auto obj : selected_objects) center_of_mass += obj->pose.position;
            center_of_mass /= float3(selected_objects.size());
            selection.position = center_of_mass;
        }

        compute_relative_transforms();
    }

    void compute_relative_transforms()
    {
        relative_transforms.clear();

        for (auto s : selected_objects)
        {
            auto t = s->pose;
            relative_transforms.push_back(selection.inverse() * s->pose);
        }
    }

public:

    editor_controller()
    {

    }

    bool selected(ObjectType & object) const
    {
        return std::find(selected_objects.begin(), selected_objects.end(), &object) != selected_objects.end();
    }

    void clear()
    {
        selected_objects.clear();
    }

    const std::vector<ObjectType *> & get_objects() const
    {
        return selected_objects;
    }

    void set_selection(const std::vector<ObjectType *> & new_selection)
    {
        selected_objects = new_selection;
    }

    void on_input(const InputEvent & event)
    {
        gizmo.handle_input(event);
        selection = to_linalg(gizmo_selection);
    }

    void on_update(const GlCamera & camera, const float2 viewport_size)
    {
        if (selected_objects.size())
        {
            gizmo.update(camera, viewport_size);
            tinygizmo::transform_gizmo("editor-controller", gizmo.gizmo_ctx, gizmo_selection);
        }

    }

    void on_draw()
    {
        if (selected_objects.size())
        {
            gizmo.draw();
        }
    }
};

struct scene_editor_app : public GLFWApp
{
    GlCamera cam;
    FlyCameraController flycam;
    ShaderMonitor shaderMonitor{ "../assets/" };
    std::unique_ptr<gui::ImGuiManager> igm;
    GlGpuTimer gpuTimer;
    std::shared_ptr<GlShader> wireframeShader;
    std::unique_ptr<editor_controller<SimpleStaticMesh>> controller;

    std::vector<SimpleStaticMesh> objects;

    scene_editor_app();
    ~scene_editor_app();

    virtual void on_window_resize(int2 size) override;
    virtual void on_input(const InputEvent & event) override;
    virtual void on_update(const UpdateEvent & e) override;
    virtual void on_draw() override;
};