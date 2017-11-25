#pragma once

#ifndef vr_renderer_hpp
#define vr_renderer_hpp

// http://efficientshading.com/2016/09/18/clustered-shading-in-the-wild/

#include "linalg_util.hpp"
#include "simple_timer.hpp"
#include "uniforms.hpp"
#include "circular_buffer.hpp"
#include "human_time.hpp"
#include "projection-math.hpp"

#include "gl-camera.hpp"
#include "gl-async-gpu-timer.hpp"
#include "gl-procedural-sky.hpp"

#include "scene.hpp"
#include "bloom_pass.hpp"
#include "shadow_pass.hpp"

using namespace avl;

enum class TextureType
{
    COLOR,
    DEPTH
};

struct RendererSettings
{
    uint32_t cameraCount = 1;
    uint32_t msaaSamples = 4;
    float2 renderSize;
    bool performanceProfiling = true;
    bool useDepthPrepass = true;
};

class PhysicallyBasedRenderer
{
    struct ViewParameter
    {
        uint32_t index;
        Pose pose;
        float4x4 viewMatrix;
        float4x4 projectionMatrix;
        float4x4 viewProjMatrix;
        float nearClip;
        float farClip;
    };

    RendererSettings settings;

    GlGpuTimer earlyZTimer;
    GlGpuTimer forwardTimer;
    GlGpuTimer shadowTimer;
    GlGpuTimer postTimer;
    GlGpuTimer renderLoopTimer;
    SimpleTimer renderLoopTimerCPU;

    SimpleTimer timer;

    GlBuffer perScene;
    GlBuffer perView;
    GlBuffer perObject;

    std::vector<ViewParameter> views;

    // MSAA 
    GlRenderbuffer multisampleRenderbuffers[2];
    GlFramebuffer multisampleFramebuffer;

    // Non-MSAA Targets
    std::vector<GlFramebuffer> eyeFramebuffers;
    std::vector<GlTexture2D> eyeTextures;
    std::vector<GlTexture2D> eyeDepthTextures;

    std::vector<Renderable *> renderSet;
    std::vector<uniforms::point_light> pointLights;

    uniforms::directional_light sunlight;
    ProceduralSky * skybox{ nullptr };

    std::unique_ptr<BloomPass> bloom;
    std::unique_ptr<StableCascadedShadowPass> shadow;

    GlShaderHandle earlyZPass = { "depth-prepass" };

    // Update per-object uniform buffer
    void update_per_object_uniform_buffer(Renderable * top, const ViewParameter & d);

    void run_depth_prepass(const ViewParameter & d);
    void run_skybox_pass(const ViewParameter & d);
    void run_shadow_pass(const ViewParameter & d);
    void run_forward_pass(std::vector<Renderable *> & renderQueueMaterial, std::vector<Renderable *> & renderQueueDefault, const ViewParameter & d);
    void run_post_pass(const ViewParameter & d);

public:

    CircularBuffer<float> earlyZAverage = { 3 };
    CircularBuffer<float> forwardAverage = { 3 };
    CircularBuffer<float> shadowAverage = { 3 };
    CircularBuffer<float> postAverage = { 3 };
    CircularBuffer<float> frameAverage = { 3 };
    CircularBuffer<float> frameAverageCPU = { 3 };

    PhysicallyBasedRenderer(const RendererSettings settings);
    ~PhysicallyBasedRenderer();

    void render_frame();

    void add_camera(const GlCamera & cam, const uint32_t index)
    {
        assert(index <= settings.cameraCount);
        ViewParameter v;
        v.index = index;
        v.pose = cam.get_pose();
        v.viewMatrix = cam.get_view_matrix();
        v.projectionMatrix = cam.get_projection_matrix(settings.renderSize.x / settings.renderSize.y);
        v.viewProjMatrix = mul(v.projectionMatrix, v.viewMatrix);
        const float2 nearFar = near_far_clip_from_projection(v.projectionMatrix);
        v.nearClip = nearFar.x;
        v.farClip = nearFar.y;
        views[index] = v;
    }

    void add_objects(const std::vector<Renderable *> & set)
    {
        renderSet = set;
    }

    void add_light(const uniforms::point_light & light)
    {
        pointLights.push_back(light);
    }

    void set_sunlight(uniforms::directional_light sun)
    {
        sunlight = sun;
    }

    uniforms::directional_light get_sunlight() const
    {
        return sunlight;
    }

    GLuint get_output_texture(const TextureType type, const uint32_t idx) const
    { 
        assert(idx <= settings.cameraCount);
        switch (type)
        {
            case TextureType::COLOR: return eyeTextures[idx];
            case TextureType::DEPTH: return eyeDepthTextures[idx];
        }
    }

    void set_procedural_sky(ProceduralSky * sky)
    {
        skybox = sky;
        sunlight.direction = sky->get_sun_direction();
        sunlight.color = float3(1.f, 1.0f, 1.0f);
        sunlight.amount = 1.0f;
    }

    ProceduralSky * get_procedural_sky() const
    {
        if (skybox) return skybox;
        else return nullptr;
    }

    StableCascadedShadowPass * get_shadow_pass() const
    { 
        return shadow.get(); 
    }

    BloomPass * get_bloom_pass() const 
    { 
        return  bloom.get(); 
    }
};

#endif // end vr_renderer_hpp
  