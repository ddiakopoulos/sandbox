// A refactoring of the gizmo editing utility part of Sterling Orsten's public domain scene-editing project:
// https://github.com/sgorsten/editor

#pragma once

#ifndef gizmo_h
#define gizmo_h

#include "scene.hpp"

struct IGizmo
{
    virtual void on_drag(math::float2 cursor, bool = false) = 0;
    virtual void on_release() = 0;
    virtual void on_cancel() = 0;
};

//////////////////////////////
// Public Gizmo Interface //
//////////////////////////////

class GizmoEditor
{
    enum class GizmoMode : int
    {
        Translate,
        Rotate,
        Scale
    };

    gfx::GlCamera & sceneCamera;
    std::shared_ptr<IGizmo> activeGizmo;
    GizmoMode gizmoMode = GizmoMode::Translate;
    Renderable * selectedObject = nullptr;
    
    uint32_t hotkey_translate = GLFW_KEY_1;
    uint32_t hotkey_rotate = GLFW_KEY_2;
    uint32_t hotkey_scale = GLFW_KEY_3;
    
    Renderable translationMesh;
    Renderable rotationMesh;
    Renderable scalingMesh;

    std::shared_ptr<IGizmo> make_gizmo(Raycast & rc, Renderable & object, const math::float3 & axis, const math::float2 & cursor);
    
    void make_gizmo_meshes();
    
public:
    
    GizmoEditor(gfx::GlCamera & camera);
    ~GizmoEditor();
    
    // For some editing applications it makes sense to rebind tools to different hotkeys
    void rebind_hotkeys(const uint32_t translationKey, const uint32_t rotationKey, const uint32_t scaleKey)
    {
        hotkey_translate = translationKey;
        hotkey_rotate = rotationKey;
        hotkey_scale = scaleKey;
    }
    
    // Passthrough input events along with a vector of scene models to pick against
    void handle_input(const util::InputEvent & event, std::vector<Renderable> & sceneModels);
    
    Renderable & get_gizmo_mesh();
    
    Renderable * get_selected_object() { return selectedObject; }
};

#endif