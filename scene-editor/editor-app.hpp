#include "index.hpp"
#include "gl-gizmo.hpp"

#include "../virtual-reality/material.hpp"
#include "../virtual-reality/vr_renderer.hpp"
#include "../virtual-reality/uniforms.hpp"
#include "../virtual-reality/assets.hpp"
#include "../virtual-reality/static_mesh.hpp"

inline Pose to_linalg(tinygizmo::rigid_transform & t)
{
    return{ reinterpret_cast<float4 &>(t.orientation), reinterpret_cast<float3 &>(t.position) };
}

inline tinygizmo::rigid_transform from_linalg(Pose & p)
{
    return{ reinterpret_cast<minalg::float4 &>(p.orientation), reinterpret_cast<minalg::float3 &>(p.position) };
}

template<typename ObjectType>
class editor_controller
{
    GlGizmo gizmo;
    tinygizmo::rigid_transform gizmo_selection;     // Center of mass of multiple objects or the pose of a single object

    Pose selection;
    std::vector<ObjectType *> selected_objects;     // Array of selected objects
    std::vector<Pose> relative_transforms;          // Pose of the objects relative to the selection
    
    bool hasEdited = false;

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

    bool has_edited() const
    {
        return hasEdited;
    }

    void on_input(const InputEvent & event)
    {
        gizmo.handle_input(event);
    }

    void on_update(const GlCamera & camera, const float2 viewport_size)
    {
        if (selected_objects.size())
        {
            gizmo.update(camera, viewport_size);
            hasEdited = tinygizmo::transform_gizmo("editor-controller", gizmo.gizmo_ctx, gizmo_selection);
        }

        // Perform editing updates on selected objects
        for (int i = 0; i < selected_objects.size(); ++i)
        {
            ObjectType * object = selected_objects[i];
            auto newPose = to_linalg(gizmo_selection) * relative_transforms[i];
            object->set_pose(newPose);
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
    ShaderMonitor shaderMonitor { "../assets/" };
    std::unique_ptr<gui::ImGuiManager> igm;

    std::shared_ptr<MetallicRoughnessMaterial> pbrMaterial;

    std::unique_ptr<PhysicallyBasedRenderer<1>> renderer;
    std::unique_ptr<editor_controller<StaticMesh>> controller; // fixme - only needs renderable interface

    uniforms::directional_light directionalLight;
    std::vector<uniforms::point_light> pointLights;
    std::vector<StaticMesh> objects;

    scene_editor_app();
    ~scene_editor_app();

    virtual void on_window_resize(int2 size) override;
    virtual void on_input(const InputEvent & event) override;
    virtual void on_update(const UpdateEvent & e) override;
    virtual void on_draw() override;
};