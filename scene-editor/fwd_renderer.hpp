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

enum class TextureType
{
    COLOR,
    DEPTH
};

struct RendererSettings
{
    uint32_t cameraCount = 1;
    uint32_t msaaSamples = 4;
    float2 renderSize{ 0, 0 };
    bool performanceProfiling = true;
    bool useDepthPrepass = false;
};

template<typename T>
struct profiler
{
    struct data_point
    {
        CircularBuffer<float> average;
        T timer;
    };

    std::unordered_map<std::string, data_point> dataPoints;

    bool enabled{ true };
    uint32_t numSamples;
    profiler(uint32_t numSamplesToKeep = 15) : numSamples(numSamplesToKeep) { }

    void set_enabled(bool newState) { enabled = newState; }

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
    }

    void collect()
    {
        if (!enabled) return;
        for (auto & v : dataPoints) v.second.average.put(v.second.timer.elapsed_ms());
    }
};

class PhysicallyBasedRenderer
{
    struct ViewData
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

    SimpleTimer timer;

    GlBuffer perScene;
    GlBuffer perView;
    GlBuffer perObject;

    std::vector<ViewData> views;

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
    void update_per_object_uniform_buffer(Renderable * top, const ViewData & d);

    void run_depth_prepass(const ViewData & d);
    void run_skybox_pass(const ViewData & d);
    void run_shadow_pass(const ViewData & d);
    void run_forward_pass(std::vector<Renderable *> & renderQueueMaterial, std::vector<Renderable *> & renderQueueDefault, const ViewData & d);
    void run_post_pass(const ViewData & d);

public:

    profiler<SimpleTimer> cpuProfiler;
    profiler<GlGpuTimer> gpuProfiler;

    PhysicallyBasedRenderer(const RendererSettings settings);
    ~PhysicallyBasedRenderer();

    void update();
    void render_frame();

    void add_camera(const uint32_t index, const Pose & p, const float4x4 & projectionMatrix);

    void add_objects(const std::vector<Renderable *> & set)
    {
        renderSet = set;
    }

    void add_light(const uniforms::point_light & light)
    {
        pointLights.push_back(light);
    }

    void set_sunlight(const uniforms::directional_light & sun)
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
        sunlight.color = float3(1.f);
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
  