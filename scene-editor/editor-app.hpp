#include "index.hpp"
#include "gl-gizmo.hpp"
#include "gl-imgui.hpp"

#include "../virtual-reality/material.hpp"
#include "../virtual-reality/vr_renderer.hpp"
#include "../virtual-reality/uniforms.hpp"
#include "../virtual-reality/assets.hpp"
#include "../virtual-reality/static_mesh.hpp"
#include "imgui/imgui_internal.h"

namespace ImGui
{
    static auto vector_getter = [](void* vec, int idx, const char** out_text)
    {
        auto & vector = *static_cast<std::vector<std::string>*>(vec);
        if (idx < 0 || idx >= static_cast<int>(vector.size())) { return false; }
        *out_text = vector.at(idx).c_str();
        return true;
    };

    bool Combo(const char* label, int* currIndex, std::vector<std::string>& values)
    {
        if (values.empty()) { return false; }
        return Combo(label, currIndex, vector_getter, static_cast<void*>(&values), values.size());
    }

    bool ListBox(const char* label, int* currIndex, std::vector<std::string>& values)
    {
        if (values.empty()) { return false; }
        return ListBox(label, currIndex, vector_getter, static_cast<void*>(&values), values.size());
    }

    enum SplitType : uint32_t
    {
        Left,
        Right,
        Top,
        Bottom
    };

    typedef std::pair<Bounds2D, Bounds2D> SplitRegion;

    SplitRegion Split(const Bounds2D & r, int * v, SplitType t)
    {
        ImGuiWindow * window = ImGui::GetCurrentWindowRead();
        const ImGuiID id = window->GetID(v);
        const auto & io = ImGui::GetIO();

        float2 cursor = float2(io.MousePos);

        if (GImGui->ActiveId == id)
        {
            
            // Get the current mouse position relative to the desired axis
            if (io.MouseDown[0])
            {
                uint32_t position = 0;

                switch (t)
                {
                    case Left:   position = cursor.x - r.min().x; break;
                    case Right:  position = r.max().x - cursor.x; break;
                    case Top:    position = cursor.y - r.min().y; break;
                    case Bottom: position = r.max().y - cursor.y; break;
                }

                *v = position;
            }
            else ImGui::SetActiveID(0, nullptr);
        }

        SplitRegion result = { r,r };

        switch (t)
        {
            case Left:   result.first._min.x = (result.second._max.x = r.min().x + *v) + 8; break;
            case Right:  result.first._max.x = (result.second._min.x = r.max().x - *v) - 8; break;
            case Top:    result.first._min.y = (result.second._max.y = r.min().y + *v) + 8; break;
            case Bottom: result.first._max.y = (result.second._min.y = r.max().y - *v) - 8; break;
        }

        if (r.contains(cursor) && !result.first.contains(cursor) && !result.second.contains(cursor))
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Move);
            if (io.MouseClicked[0])
            {
                ImGui::SetActiveID(id, window);
            }
        }
        return result;
    };

}

static inline Pose to_linalg(tinygizmo::rigid_transform & t)
{
    return{ reinterpret_cast<float4 &>(t.orientation), reinterpret_cast<float3 &>(t.position) };
}

static inline tinygizmo::rigid_transform from_linalg(Pose & p)
{
    return{ reinterpret_cast<minalg::float4 &>(p.orientation), reinterpret_cast<minalg::float3 &>(p.position) };
}

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

struct scene_editor_app : public GLFWApp
{
    GlCamera cam;
    FlyCameraController flycam;
    ShaderMonitor shaderMonitor { "../assets/" };
    std::unique_ptr<gui::ImGuiManager> igm;
    std::unique_ptr<HosekProceduralSky> skybox;

    std::unique_ptr<PhysicallyBasedRenderer<1>> renderer;
    std::unique_ptr<editor_controller<GameObject>> editor;

    std::shared_ptr<PointLight> lightA;
    std::shared_ptr<PointLight> lightB;
    std::shared_ptr<DirectionalLight> sun;
    std::vector<std::shared_ptr<GameObject>> objects;

    scene_editor_app();
    ~scene_editor_app();

    virtual void on_window_resize(int2 size) override;
    virtual void on_input(const InputEvent & event) override;
    virtual void on_update(const UpdateEvent & e) override;
    virtual void on_draw() override;
};