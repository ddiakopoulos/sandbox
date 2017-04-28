#pragma once

#ifndef vr_renderer_hpp
#define vr_renderer_hpp

#include "linalg_util.hpp"
#include "geometric.hpp"
#include "geometry.hpp"
#include "scene.hpp"
#include "camera.hpp"
#include "gpu_timer.hpp"
#include "procedural_mesh.hpp"
#include "simple_timer.hpp"
#include "avl_imgui.hpp"
#include "uniforms.hpp"
#include "debug_line_renderer.hpp"
#include "bloom_pass.hpp"
#include "shadow_pass.hpp"
#include "procedural_sky.hpp"

/*
 * To Do - 3.25.2017
 * [ ] Shadow pass should be rendered from cyclops eye
 * [ ] Clean up RenderPassData struct
 * [ ] Better naming all-around
 * [ ] ImGui controls for all passes
 * [ ] Volume/height fog
 * [ ] Built-in wireframe renderer
*/

using namespace avl;

struct SkyboxPass
{
    HosekProceduralSky skybox;
    void execute(const float3 & eye, const float4x4 & viewProj, const float farClip)
    {
        skybox.render(viewProj, eye, farClip);
    }
};

enum class Eye : int
{
    LeftEye = 0, 
    RightEye = 1
};

struct EyeData
{
    Pose pose;
    float4x4 projectionMatrix;
    float4x4 viewMatrix;
    float4x4 viewProjMatrix;
};

struct ShadowData
{
    GLuint csmArrayHandle;
    float3 directionalLight;
};

struct LightCollection
{
    uniforms::directional_light * directionalLight;
    std::vector<uniforms::point_light *> pointLights;
    //std::vector<uniforms::spot_light *> spotLights;
};

struct RenderPassData
{
    const int & eye;
    const EyeData & data;
    const ShadowData & shadow;
    RenderPassData(const int & eye, const EyeData & data, const ShadowData & shadowData)
        : eye(eye), data(data), shadow(shadowData) {}
};

class VR_Renderer
{
    std::vector<DebugRenderable *> debugSet;

    std::vector<Renderable *> renderSet;
    LightCollection lights;

    float2 renderSizePerEye;
    GlGpuTimer renderTimer;
    SimpleTimer timer;

    GlBuffer perScene;
    GlBuffer perView;

    EyeData eyes[2];

    GlFramebuffer eyeFramebuffers[2];
    GlTexture2D eyeTextures[2];
    GlRenderbuffer multisampleRenderbuffers[2];
    GlFramebuffer multisampleFramebuffer;

    GLuint outputTextureHandles[2] = { 0, 0 };

    void run_skybox_pass(const RenderPassData & d);
    void run_forward_pass(const RenderPassData & d);
    void run_forward_wireframe_pass(const RenderPassData & d);
    void run_shadow_pass(const RenderPassData & d);

    void run_post_pass(const RenderPassData & d);

    void run_bloom_pass(const RenderPassData & d);
    void run_reflection_pass(const RenderPassData & d);
    void run_ssao_pass(const RenderPassData & d);
    void run_smaa_pass(const RenderPassData & d);
    void run_blackout_pass(const RenderPassData & d);

    bool renderPost{ true };
    bool renderWireframe { false };
    bool renderShadows { true };
    bool renderBloom { false };
    bool renderReflection { false };
    bool renderSSAO { false };
    bool renderSMAA { false };
    bool renderBlackout { false };

public:

    std::unique_ptr<SkyboxPass> skybox;
    std::unique_ptr<BloomPass> bloom;
    std::unique_ptr<ShadowPass> shadow;

    DebugLineRenderer sceneDebugRenderer;

    VR_Renderer(float2 renderSizePerEye);
    ~VR_Renderer();

    void render_frame();

    void set_eye_data(const EyeData left, const EyeData right);

    GLuint get_eye_texture(const Eye e) { return outputTextureHandles[(int)e]; }

    // A `DebugRenderable` is for rapid prototyping, exposing a single `draw(viewProj)` interface.
    // The list of these is drawn before `Renderable` objects, where they use their own shading
    // programs and emit their own draw calls.
    void add_debug_renderable(DebugRenderable * object) { debugSet.push_back(object); }

    // These lists are small for now and easily copyable: 

    // A `Renderable` is a generic interface for this engine, appropriate for use with
    // the material system and all customizations (frustum culling, etc)
    void add_renderables(const std::vector<Renderable *> & set) { renderSet = set; }

    void set_lights(const LightCollection & collection) { lights = collection; }
};

#endif // end vr_renderer_hpp
  