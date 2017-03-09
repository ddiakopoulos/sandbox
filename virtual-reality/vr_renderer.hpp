#pragma once

#ifndef vr_renderer_hpp
#define vr_renderer_hpp

#include "linalg_util.hpp"
#include "geometric.hpp"
#include "geometry.hpp"
#include "scene.hpp"
#include "camera.hpp"
#include "gpu_timer.hpp"

class DebugLineRenderer : public DebugRenderable
{
    struct Vertex { float3 position; float3 color; };
    std::vector<Vertex> vertices;
    GlMesh debugMesh;
    GlShader debugShader;

    Geometry axis = make_axis();
    Geometry box = make_cube();
    Geometry sphere = make_sphere(1.f);

    constexpr static const char debugVertexShader[] = R"(#version 330 
        layout(location = 0) in vec3 v; 
        layout(location = 1) in vec3 c; 
        uniform mat4 u_mvp; 
        out vec3 oc; 
        void main() { gl_Position = u_mvp * vec4(v.xyz, 1); oc = c; }
    )";

    constexpr static const char debugFragmentShader[] = R"(#version 330 
        in vec3 oc; 
        out vec4 f_color; 
        void main() { f_color = vec4(oc.rgb, 1); }
    )";

public:

    DebugLineRenderer()
    {
        debugShader = GlShader(debugVertexShader, debugFragmentShader);
    }

    virtual void draw(const float4x4 & viewProj) override
    {
        debugMesh.set_vertices(vertices.size(), vertices.data(), GL_DYNAMIC_DRAW);
        debugMesh.set_attribute(0, &Vertex::position);
        debugMesh.set_attribute(1, &Vertex::color);
        debugMesh.set_non_indexed(GL_LINES);

        auto model = Identity4x4;
        auto modelViewProjectionMatrix = mul(viewProj, model);

        debugShader.bind();
        debugShader.uniform("u_mvp", modelViewProjectionMatrix);
        debugMesh.draw_elements();
        debugShader.unbind();
    }

    void clear()
    {
        vertices.clear();
    }

    // Coordinates should be provided pre-transformed to world-space
    void draw_line(const float3 & from, const float3 & to, const float3 color = float3(1, 1, 1))
    {
        vertices.push_back({ from, color });
        vertices.push_back({ to, color });
    }

    void draw_box(const Pose & pose, const float & half, const float3 color = float3(1, 1, 1))
    {
        // todo - apply exents and position
        for (const auto v : box.vertices) vertices.push_back({ v, color });
    }

    void draw_sphere(const Pose & pose, const float & radius, const float3 color = float3(1, 1, 1))
    {
        // todo - apply radius and position
        for (const auto v : sphere.vertices) vertices.push_back({ v, color });
    }

    void draw_axis(const Pose & pose, const float3 color = float3(1, 1, 1))
    {
        for (int i = 0; i < axis.vertices.size(); ++i)
        {
            auto v = axis.vertices[i];
            v = pose.transform_coord(v);
            vertices.push_back({ v, axis.colors[i].xyz() });
        }
    }

    void draw_frustum(const float4x4 & view, const float3 color = float3(1, 1, 1))
    {
        Frustum f(view);
        for (auto p : f.get_corners()) vertices.push_back({ p, color });
    }

};

using namespace avl;

#if defined(ANVIL_PLATFORM_WINDOWS)
    #define ALIGNED(n) __declspec(align(n))
#else
    #define ALIGNED(n) alignas(n)
#endif

namespace uniforms
{
    // Add resolution
    struct per_scene
    {
        static const int      binding = 0;
        float                 time;
    };

    struct per_view
    {
        static const int      binding = 1;
        ALIGNED(16) float4x4  view;
        ALIGNED(16) float4x4  viewProj;
        ALIGNED(16) float3    eyePos;
    };

    struct directional_light
    {
        ALIGNED(16) float3    color;
        ALIGNED(16) float3    direction;
        ALIGNED(16) float     size;
    };

    struct point_light
    {
        ALIGNED(16) float3    color;
        ALIGNED(16) float3    position;
        ALIGNED(16) float3    attenuation; // constant, linear, quadratic
    };

    struct spot_light
    {
        ALIGNED(16) float3    color;
        ALIGNED(16) float3    direction;
        ALIGNED(16) float3    position;
        ALIGNED(16) float3    attenuation; // constant, linear, quadratic
        ALIGNED(16) float     cutoff;
    };
}

enum class Eye : int
{
    LeftEye = 0, 
    RightEye = 1
};

struct EyeData
{
    Pose pose;
    float4x4 projectionMatrix;
};

struct LightSet
{
    uniforms::directional_light * directionalLight;
    std::vector<uniforms::point_light *> pointLights;
    std::vector<uniforms::spot_light *> spotLights;
};

class VR_Renderer
{
    std::vector<Renderable *> renderSet;
    std::vector<DebugRenderable *> debugSet;

    LightSet * lightSet;

    float2 renderSizePerEye;
    GlGpuTimer renderTimer;

    GlBuffer perScene;
    GlBuffer perView;

    EyeData eyes[2];

    GlFramebuffer eyeFramebuffers[2];
    GlTexture2D eyeTextures[2];
    GlRenderbuffer multisampleRenderbuffers[2];
    GlFramebuffer multisampleFramebuffer;

    void run_skybox_pass();
    void run_forward_pass(const uniforms::per_view & uniforms);
    void run_forward_wireframe_pass();
    void run_shadow_pass();

    void run_bloom_pass();
    void run_reflection_pass();
    void run_ssao_pass();
    void run_smaa_pass();
    void run_blackout_pass();

    void run_post_pass();

    bool renderWireframe { false };
    bool renderShadows { false };
    bool renderPost { false };
    bool renderBloom { false };
    bool renderReflection { false };
    bool renderSSAO { false };
    bool renderSMAA { false };
    bool renderBlackout { false };

public:

    DebugLineRenderer sceneDebugRenderer;

    VR_Renderer(float2 renderSizePerEye);
    ~VR_Renderer();

    void render_frame();

    void set_eye_data(const EyeData left, const EyeData right);

    GLuint get_eye_texture(Eye e) { return eyeTextures[(int) e]; }

    // A `Renderable` is a generic interface for this engine, appropriate for use with
    // the material system and all customizations (frustum culling, etc)
    void add_renderable(Renderable * object) { renderSet.push_back(object); }

    // A `DebugRenderable` is for rapid prototyping, exposing a single `draw(viewProj)` interface.
    // The list of these is drawn before `Renderable` objects, where they use their own shading
    // programs and emit their own draw calls.
    void add_debug_renderable(DebugRenderable * object) { debugSet.push_back(object); }
};

#endif // end vr_renderer_hpp
  