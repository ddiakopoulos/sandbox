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

using namespace avl;

class DebugLineRenderer : public DebugRenderable
{
    struct Vertex { float3 position; float3 color; };
    std::vector<Vertex> vertices;
    GlMesh debugMesh;
    GlShader debugShader;

    Geometry axis = make_axis();
    Geometry box = make_cube();
    Geometry sphere = make_sphere(0.1f);

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
        // todo - apply exents
        for (const auto v : box.vertices)
        {
            auto tV = pose.transform_coord(v);
            vertices.push_back({ tV, color });
        }
    }

    void draw_sphere(const Pose & pose, const float & radius, const float3 color = float3(1, 1, 1))
    {
        // todo - apply radius
        for (const auto v : sphere.vertices)
        {
            auto tV = pose.transform_coord(v);
            vertices.push_back({ tV, color });
        }
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

// https://www.khronos.org/opengl/wiki/Interface_Block_(GLSL)#Memory_layout

namespace uniforms
{
    static const int MAX_POINT_LIGHTS = 4;

    struct point_light
    {
        ALIGNED(16) float3    color;
        ALIGNED(16) float3    position;
        float       radius;
    };

    struct directional_light
    {
        ALIGNED(16) float3    color;
        ALIGNED(16) float3    direction;
        float                 amount; // constant
    };

    struct spot_light
    {
        ALIGNED(16) float3    color;
        ALIGNED(16) float3    direction;
        ALIGNED(16) float3    position;
        ALIGNED(16) float3    attenuation; // constant, linear, quadratic
        float                 cutoff;
    };

    // Add resolution
    struct per_scene
    {
        static const int     binding = 0;
        directional_light    directional_light;
        point_light          point_lights[MAX_POINT_LIGHTS];
        float                time;
        int                  activePointLights;
        ALIGNED(8) float2    resolution;
        ALIGNED(8) float2    invResolution;
    };

    struct per_view
    {
        static const int      binding = 1;
        ALIGNED(16) float4x4  view;
        ALIGNED(16) float4x4  viewProj;
        ALIGNED(16) float3    eyePos;
    };
}

struct BloomPass
{
    union 
    {
        GLuint pipelines[1];
        struct  { GLuint downsample_pipeline; };
    };

    float middleGrey = 1.0f;
    float whitePoint = 1.5f;
    float threshold = 0.66f;

    GlShader hdr_post;
    GlShader hdr_lumShader;
    GlShader hdr_avgLumShader;
    GlShader hdr_blurShader;
    GlShader hdr_brightShader;
    GlShader hdr_tonemapShader;

    GlFramebuffer luminance_0, luminance_1, luminance_2, luminance_3, luminance_4, brightFramebuffer, blurFramebuffer, outputFramebuffer;
    GlTexture2D luminanceTex_0, luminanceTex_1, luminanceTex_2, luminanceTex_3, luminanceTex_4, brightTex, blurTex, outputTex;

    GlMesh fsQuad;

    float2 perEyeSize;

    BloomPass(float2 size) : perEyeSize(size)
    {
        fsQuad = make_fullscreen_quad();

        luminanceTex_0.setup(128, 128, GL_RGBA, GL_RGBA, GL_FLOAT, nullptr);
        luminanceTex_1.setup(64, 64,   GL_RGBA, GL_RGBA, GL_FLOAT, nullptr);
        luminanceTex_2.setup(16, 16,   GL_RGBA, GL_RGBA, GL_FLOAT, nullptr);
        luminanceTex_3.setup(4, 4,     GL_RGBA, GL_RGBA, GL_FLOAT, nullptr);
        luminanceTex_4.setup(1, 1,     GL_RGBA, GL_RGBA, GL_FLOAT, nullptr);
        brightTex.setup(perEyeSize.x / 2, perEyeSize.y / 2, GL_RGBA, GL_RGBA, GL_FLOAT, nullptr);
        blurTex.setup(perEyeSize.x / 8, perEyeSize.y / 8, GL_RGBA, GL_RGBA, GL_FLOAT, nullptr);
        outputTex.setup(perEyeSize.x, perEyeSize.y, GL_RGBA, GL_RGBA, GL_FLOAT, nullptr);

        glNamedFramebufferTexture2DEXT(luminance_0, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, luminanceTex_0, 0);
        glNamedFramebufferTexture2DEXT(luminance_1, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, luminanceTex_1, 0);
        glNamedFramebufferTexture2DEXT(luminance_2, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, luminanceTex_2, 0);
        glNamedFramebufferTexture2DEXT(luminance_3, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, luminanceTex_3, 0);
        glNamedFramebufferTexture2DEXT(luminance_4, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, luminanceTex_4, 0);
        glNamedFramebufferTexture2DEXT(brightFramebuffer, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brightTex, 0);
        glNamedFramebufferTexture2DEXT(blurFramebuffer, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, blurTex, 0);
        glNamedFramebufferTexture2DEXT(outputFramebuffer, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outputTex, 0);

        luminance_0.check_complete();
        luminance_1.check_complete();
        luminance_2.check_complete();
        luminance_3.check_complete();
        luminance_4.check_complete();
        brightFramebuffer.check_complete();
        blurFramebuffer.check_complete();
        outputFramebuffer.check_complete();

        hdr_post = GlShader(GL_VERTEX_SHADER, read_file_text("../assets/shaders/hdr/hdr_post_vert.glsl"));
        hdr_lumShader = GlShader(read_file_text("../assets/shaders/hdr/hdr_post_vert.glsl"), read_file_text("../assets/shaders/hdr/hdr_lum_frag.glsl"));
        hdr_avgLumShader = GlShader(GL_FRAGMENT_SHADER, read_file_text("../assets/shaders/hdr/hdr_lumavg_frag.glsl"));
        hdr_blurShader = GlShader(read_file_text("../assets/shaders/hdr/hdr_blur_vert.glsl"), read_file_text("../assets/shaders/hdr/hdr_blur_frag.glsl"));
        hdr_brightShader = GlShader(read_file_text("../assets/shaders/hdr/hdr_post_vert.glsl"), read_file_text("../assets/shaders/hdr/hdr_bright_frag.glsl"));
        hdr_tonemapShader = GlShader(read_file_text("../assets/shaders/hdr/hdr_tonemap_vert.glsl"), read_file_text("../assets/shaders/hdr/hdr_tonemap_frag.glsl"));

        glCreateProgramPipelines(GLsizei(1), pipelines);
        glBindProgramPipeline(downsample_pipeline);
        glUseProgramStages(downsample_pipeline, GL_VERTEX_SHADER_BIT, hdr_post.handle());
        glUseProgramStages(downsample_pipeline, GL_FRAGMENT_SHADER_BIT, hdr_avgLumShader.handle());

        gl_check_error(__FILE__, __LINE__);
    }

    ~BloomPass() 
    {
        glDeleteProgramPipelines(GLsizei(1), pipelines);
    }

    void execute(const GlTexture2D & sceneColorTex)
    {
        // Disable culling and depth testing for post processing
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_FRAMEBUFFER_SRGB);

        glBindFramebuffer(GL_FRAMEBUFFER, luminance_0); // 128x128 surface area - calculate luminance 
        glViewport(0, 0, 128, 128);
        hdr_lumShader.bind();
        hdr_lumShader.texture("s_texColor", 0, sceneColorTex, GL_TEXTURE_2D);
        hdr_lumShader.uniform("u_modelViewProj", Identity4x4);
        fsQuad.draw_elements();
        hdr_lumShader.unbind();

        {
            //hdr_avgLumShader.bind();

            glActiveShaderProgram(downsample_pipeline, hdr_avgLumShader.handle());
            glBindProgramPipeline(downsample_pipeline);

            glBindFramebuffer(GL_FRAMEBUFFER, luminance_1);// 64x64 surface area - downscale + average
            glViewport(0, 0, 64, 64);
            hdr_avgLumShader.texture("s_texColor", 0, luminanceTex_0, GL_TEXTURE_2D);
            hdr_avgLumShader.uniform("u_modelViewProj", Identity4x4);
            fsQuad.draw_elements();

            glBindFramebuffer(GL_FRAMEBUFFER, luminance_2); // 16x16 surface area - downscale + average
            glViewport(0, 0, 16, 16);
            hdr_avgLumShader.texture("s_texColor", 0, luminanceTex_1, GL_TEXTURE_2D);
            hdr_avgLumShader.uniform("u_modelViewProj", Identity4x4);
            fsQuad.draw_elements();

            glBindFramebuffer(GL_FRAMEBUFFER, luminance_3); // 4x4 surface area - downscale + average
            glViewport(0, 0, 4, 4);
            hdr_avgLumShader.texture("s_texColor", 0, luminanceTex_2, GL_TEXTURE_2D);
            hdr_avgLumShader.uniform("u_modelViewProj", Identity4x4);
            fsQuad.draw_elements();

            glBindFramebuffer(GL_FRAMEBUFFER, luminance_4); // 1x1 surface area - downscale + average
            glViewport(0, 0, 1, 1);
            hdr_avgLumShader.texture("s_texColor", 0, luminanceTex_3, GL_TEXTURE_2D);
            hdr_avgLumShader.uniform("u_modelViewProj", Identity4x4);
            fsQuad.draw_elements();

            glBindProgramPipeline(0);
        }

        gl_check_error(__FILE__, __LINE__);

        // Readback luminance value
        // std::vector<float> lumValue = { 0, 0, 0, 0 };
        //glActiveTexture(GL_TEXTURE0);
        //glBindTexture(GL_TEXTURE_2D, luminanceTex_4.get_gl_handle());
        //glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, lumValue.data());

        float4 tonemap = { middleGrey, whitePoint * whitePoint, threshold, 0.0f };

        glBindFramebuffer(GL_FRAMEBUFFER, brightFramebuffer);
        glViewport(0, 0, perEyeSize.x / 2, perEyeSize.y / 2);
        hdr_brightShader.bind();
        hdr_brightShader.texture("s_texColor", 0, sceneColorTex, GL_TEXTURE_2D);
        hdr_brightShader.texture("s_texLum", 1, luminanceTex_4, GL_TEXTURE_2D); // 1x1
        hdr_brightShader.uniform("u_tonemap", tonemap);
        hdr_brightShader.uniform("u_modelViewProj", Identity4x4);
        fsQuad.draw_elements();
        hdr_brightShader.unbind();

        glBindFramebuffer(GL_FRAMEBUFFER, blurFramebuffer);
        glViewport(0, 0, perEyeSize.x / 8, perEyeSize.y / 8);
        hdr_blurShader.bind();
        hdr_blurShader.texture("s_texColor", 0, brightTex, GL_TEXTURE_2D);
        hdr_blurShader.uniform("u_viewTexel", float2(1.f / (perEyeSize.x / 8.f), 1.f / (perEyeSize.y / 8.f)));
        hdr_blurShader.uniform("u_modelViewProj", Identity4x4);
        fsQuad.draw_elements();
        hdr_blurShader.unbind();

        //glBlendEquation(GL_FUNC_ADD);
        //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        //glEnable(GL_BLEND);

        glBindFramebuffer(GL_FRAMEBUFFER, outputFramebuffer);
        glViewport(0, 0, perEyeSize.x, perEyeSize.y);
        hdr_tonemapShader.bind();
        hdr_tonemapShader.texture("s_texColor", 0, sceneColorTex, GL_TEXTURE_2D);
        hdr_tonemapShader.texture("s_texLum", 1, luminanceTex_4, GL_TEXTURE_2D); // 1x1
        hdr_tonemapShader.texture("s_texBlur", 2, blurTex, GL_TEXTURE_2D);
        hdr_tonemapShader.uniform("u_tonemap", tonemap);
        hdr_tonemapShader.uniform("u_modelViewProj", Identity4x4);
        hdr_tonemapShader.uniform("u_viewTexel", float2(1.f / (float)perEyeSize.x, 1.f / (float)perEyeSize.y));
        fsQuad.draw_elements();
        hdr_tonemapShader.unbind();

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_FRAMEBUFFER_SRGB);
    }

    GLuint get_output_texture() const { return outputTex.id(); }

    GLuint get_luminance_texture() const { return luminanceTex_1.id(); }

    GLuint get_bright_tex() const { return brightTex.id(); }

    GLuint get_blur_tex() const { return blurTex.id(); }
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
};

struct LightCollection
{
    uniforms::directional_light * directionalLight;
    std::vector<uniforms::point_light *> pointLights;
    //std::vector<uniforms::spot_light *> spotLights;
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

    bool renderPost{ true };
    bool renderWireframe { false };
    bool renderShadows { false };
    bool renderBloom { true };
    bool renderReflection { false };
    bool renderSSAO { false };
    bool renderSMAA { false };
    bool renderBlackout { false };

public:

    std::unique_ptr<BloomPass> leftBloom, rightBloom;

    DebugLineRenderer sceneDebugRenderer;

    VR_Renderer(float2 renderSizePerEye);
    ~VR_Renderer();

    void render_frame();

    void set_eye_data(const EyeData left, const EyeData right);

    GLuint get_eye_texture(const Eye e) 
    { 
        return outputTextureHandles[(int)e];
    }

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
  