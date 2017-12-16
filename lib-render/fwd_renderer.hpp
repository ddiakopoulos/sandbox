#pragma once

#ifndef vr_renderer_hpp
#define vr_renderer_hpp

#include "math-core.hpp"
#include "simple_timer.hpp"
#include "uniforms.hpp"
#include "circular_buffer.hpp"
#include "human_time.hpp"

#include "gl-camera.hpp"
#include "gl-async-gpu-timer.hpp"
#include "gl-procedural-sky.hpp"

#include "scene.hpp"
#include "bloom_pass.hpp"
#include "shadow_pass.hpp"

using namespace avl;

struct renderer_settings
{
    float2 renderSize{ 0, 0 };
    int cameraCount = 1;
    int msaaSamples = 4;
    bool performanceProfiling = true;
    bool useDepthPrepass = false;
    bool bloomEnabled = true;
    bool shadowsEnabled = true;
};

struct view_data
{
    uint32_t index;
    Pose pose;
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
    float4x4 viewProjMatrix;
    float nearClip;
    float farClip;
};

inline view_data make_view_data(const uint32_t index, const Pose & p, const float4x4 & projectionMatrix)
{
    view_data v;
    v.index = index;
    v.pose = p;
    v.viewMatrix = p.view_matrix();
    v.projectionMatrix = projectionMatrix;
    v.viewProjMatrix = mul(v.projectionMatrix, v.viewMatrix);
    near_far_clip_from_projection(v.projectionMatrix, v.nearClip, v.farClip);
    return v;
}

struct scene_data
{
    ProceduralSky * skybox{ nullptr };
    std::vector<Renderable *> renderSet;
    std::vector<uniforms::point_light> pointLights;
    uniforms::directional_light sunlight;
    std::vector<view_data> views;
};

template<typename T>
struct profiler
{
    struct data_point
    {
        CircularBuffer<double> average;
        T timer;
    };

    std::unordered_map<std::string, data_point> dataPoints;

    bool enabled{ true };
    uint32_t numSamples;
    profiler(uint32_t numSamplesToKeep = 5) : numSamples(numSamplesToKeep) { }

    void set_enabled(bool newState) 
    { 
        enabled = newState;
        dataPoints.clear();
    }

    void begin(const std::string & id)
    { 
        if (!enabled) return;
        auto pt = dataPoints.insert({ id, {} });
        if (pt.second == true) pt.first->second.average.resize(numSamples);
        dataPoints[id].timer.start();
    }

    void end(const std::string & id) 
    { 
        if (!enabled) return;
        dataPoints[id].timer.stop();
        double t = dataPoints[id].timer.elapsed_ms();
        if (t > 0) dataPoints[id].average.put(t);
    }
};

class PhysicallyBasedRenderer
{
    SimpleTimer timer;

    GlBuffer perScene;
    GlBuffer perView;
    GlBuffer perObject;

    // MSAA 
    GlRenderbuffer multisampleRenderbuffers[2];
    GlFramebuffer multisampleFramebuffer;

    // Non-MSAA Targets
    std::vector<GlFramebuffer> eyeFramebuffers;
    std::vector<GlTexture2D> eyeTextures;
    std::vector<GlTexture2D> eyeDepthTextures;

    scene_data scene;

    std::unique_ptr<BloomPass> bloom;
    std::unique_ptr<StableCascadedShadowPass> shadow;

    GlShaderHandle earlyZPass = { "depth-prepass" };

    // Update per-object uniform buffer
    void update_per_object_uniform_buffer(Renderable * top, const view_data & d);

    void run_depth_prepass(const view_data & d);
    void run_skybox_pass(const view_data & d);
    void run_shadow_pass(const view_data & d);
    void run_forward_pass(std::vector<Renderable *> & renderQueueMaterial, std::vector<Renderable *> & renderQueueDefault, const ViewData & d);
    void run_post_pass(const view_data & d);

public:

    renderer_settings settings;
    profiler<SimpleTimer> cpuProfiler;
    profiler<GlGpuTimer> gpuProfiler;

    PhysicallyBasedRenderer(const renderer_settings & settings);
    ~PhysicallyBasedRenderer();

    void render_frame(const scene_data & scene);

    uint32_t get_color_texture(const int32_t idx) const;
    uint32_t get_depth_texture(const int32_t idx) const;

    StableCascadedShadowPass & get_shadow_pass() const;
    BloomPass & get_bloom_pass() const;
};

template<class F> void visit_fields(PhysicallyBasedRenderer & o, F f)
{
    f("num_cameras", o.settings.cameraCount);
    f("num_msaa_samples", o.settings.msaaSamples);
    f("render_size", o.settings.renderSize);
    f("performance_profiling", o.settings.performanceProfiling);
    f("depth_prepass", o.settings.useDepthPrepass);
    f("bloom_pass", o.settings.bloomEnabled);
    f("shadow_pass", o.settings.shadowsEnabled);
};

#endif // end vr_renderer_hpp
  