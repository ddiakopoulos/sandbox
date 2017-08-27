#pragma once

#ifndef vr_renderer_hpp
#define vr_renderer_hpp

#include "projection-math.hpp"
#include "linalg_util.hpp"
#include "geometric.hpp"
#include "geometry.hpp"
#include "procedural_mesh.hpp"
#include "simple_timer.hpp"
#include "uniforms.hpp"
#include "debug_line_renderer.hpp" 

#include "gl-scene.hpp"
#include "gl-camera.hpp"
#include "gl-async-gpu-timer.hpp"
#include "gl-procedural-sky.hpp"
#include "gl-imgui.hpp"

#include "bloom_pass.hpp"
#include "shadow_pass.hpp"

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
    RightEye = 1,
    CenterEye = 2
};

struct EyeData
{
    Pose pose;
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
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
    RenderPassData(const int & eye, const EyeData & data, const ShadowData & shadowData) : eye(eye), data(data), shadow(shadowData) {}
};

template<uint32_t NumEyes>
class PhysicallyBasedRenderer
{
    std::vector<Renderable *> renderSet;
    LightCollection lights;

    float2 renderSizePerEye;
    GlGpuTimer renderTimer;
    SimpleTimer timer;

    GlBuffer perScene;
    GlBuffer perView;

    EyeData eyes[NumEyes];

    GlFramebuffer eyeFramebuffers[NumEyes];
    GlTexture2D eyeTextures[NumEyes];
    GlRenderbuffer multisampleRenderbuffers[NumEyes];
    GlFramebuffer multisampleFramebuffer;

    GLuint outputTextureHandles[NumEyes];

    void run_skybox_pass(const RenderPassData & d)
    {
        glDisable(GL_DEPTH_TEST);
        skybox->execute(d.data.pose.position, d.data.viewProjMatrix, near_far_clip_from_projection(d.data.projectionMatrix).y);
        glDisable(GL_DEPTH_TEST);
    }

    void run_shadow_pass(const RenderPassData & d)
    {
        const float2 nearFarClip = near_far_clip_from_projection(d.data.projectionMatrix);

        shadow->update_cascades(make_view_matrix_from_pose(d.data.pose),
            nearFarClip.x,
            nearFarClip.y,
            aspect_from_projection(d.data.projectionMatrix),
            vfov_from_projection(d.data.projectionMatrix),
            d.shadow.directionalLight);

        shadow->pre_draw();

        for (Renderable * obj : renderSet)
        {
            // Fixme - check if should cast shadow
            const float4x4 modelMatrix = mul(obj->get_pose().matrix(), make_scaling_matrix(obj->get_scale()));
            shadow->cascadeShader.uniform("u_modelMatrix", modelMatrix);
            obj->draw();
        }

        shadow->post_draw();
    }

    void run_forward_pass(const RenderPassData & d)
    {
        sceneDebugRenderer.draw(d.data.viewProjMatrix);
        for (auto obj : debugSet) { obj->draw(d.data.viewProjMatrix); }

        // This is done per-eye but should be done per frame instead...
        auto renderSortFunc = [&d](Renderable * lhs, Renderable * rhs)
        {
            auto lid = lhs->get_material()->id();
            auto rid = rhs->get_material()->id();

            auto cameraPositionWS = d.data.pose.position;
            auto lDist = distance(cameraPositionWS, lhs->get_pose().position);
            auto rDist = distance(cameraPositionWS, rhs->get_pose().position);

            if (lid != rid) return lid > rid;
            else return lDist < rDist;
        };

        std::priority_queue<Renderable *, std::vector<Renderable*>, decltype(renderSortFunc)> renderQueue(renderSortFunc);

        for (auto obj : renderSet) { renderQueue.push(obj); }

        while (!renderQueue.empty())
        {
            auto top = renderQueue.top();
            Material * mat = top->get_material();
            mat->update_uniforms(&d);
            mat->use(mul(top->get_pose().matrix(), make_scaling_matrix(top->get_scale())), d.data.viewMatrix);
            top->draw();
            renderQueue.pop();
        }

        // Refactor this
        outputTextureHandles[0] = eyeTextures[0];
        outputTextureHandles[1] = eyeTextures[1];
    }

    void run_post_pass(const RenderPassData & d)
    {
        if (!renderPost) return;
        if (renderBloom) run_bloom_pass(d);
    }

    void run_bloom_pass(const RenderPassData & d)
    {
        bloom->execute(eyeTextures[d.eye]);
        glBlitNamedFramebuffer(bloom->get_output_texture(), eyeTextures[d.eye], 0, 0, renderSizePerEye.x, renderSizePerEye.y, 0, 0, renderSizePerEye.x, renderSizePerEye.y, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    }

    bool renderPost{ true };
    bool renderShadows { true };
    bool renderBloom { false };

public:

    std::unique_ptr<SkyboxPass> skybox;
    std::unique_ptr<BloomPass> bloom;
    std::unique_ptr<ShadowPass> shadow;

    PhysicallyBasedRenderer(float2 renderSizePerEye)
    {
        // Generate multisample render buffers for color and depth, attach to multi-sampled framebuffer target
        glNamedRenderbufferStorageMultisampleEXT(multisampleRenderbuffers[0], 4, GL_RGBA8, renderSizePerEye.x, renderSizePerEye.y);
        glNamedRenderbufferStorageMultisampleEXT(multisampleRenderbuffers[1], 4, GL_DEPTH_COMPONENT, renderSizePerEye.x, renderSizePerEye.y);
        glNamedFramebufferRenderbufferEXT(multisampleFramebuffer, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, multisampleRenderbuffers[0]);
        glNamedFramebufferRenderbufferEXT(multisampleFramebuffer, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, multisampleRenderbuffers[1]);
        if (glCheckNamedFramebufferStatusEXT(multisampleFramebuffer, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) throw std::runtime_error("Framebuffer incomplete!");

        // Generate textures and framebuffers for `NumEyes`
        for (int eyeIndex = 0; eyeIndex < NumEyes; ++eyeIndex)
        {
            glTextureImage2DEXT(eyeTextures[eyeIndex], GL_TEXTURE_2D, 0, GL_RGBA8, renderSizePerEye.x, renderSizePerEye.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTextureParameteriEXT(eyeTextures[eyeIndex], GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTextureParameteriEXT(eyeTextures[eyeIndex], GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTextureParameteriEXT(eyeTextures[eyeIndex], GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTextureParameteriEXT(eyeTextures[eyeIndex], GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTextureParameteriEXT(eyeTextures[eyeIndex], GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
            glNamedFramebufferTexture2DEXT(eyeFramebuffers[eyeIndex], GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, eyeTextures[eyeIndex], 0);
            eyeFramebuffers[eyeIndex].check_complete();
        }

        skybox.reset(new SkyboxPass());
        shadow.reset(new ShadowPass());
        bloom.reset(new BloomPass(renderSizePerEye));

        timer.start();
    }

    ~PhysicallyBasedRenderer()
    {
        timer.stop();
    }

    void render_frame()
    {
        // Renderer default state
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);

        glBindBufferBase(GL_UNIFORM_BUFFER, uniforms::per_scene::binding, perScene);
        glBindBufferBase(GL_UNIFORM_BUFFER, uniforms::per_view::binding, perView);

        // Update per-scene uniform buffer
        uniforms::per_scene b = {};
        b.time = timer.milliseconds().count();
        b.resolution = renderSizePerEye;
        b.invResolution = 1.f / b.resolution;
        b.activePointLights = lights.pointLights.size();

        b.directional_light.color = lights.directionalLight->color;
        b.directional_light.direction = lights.directionalLight->direction;
        b.directional_light.amount = lights.directionalLight->amount;
        for (int i = 0; i < (int)std::min(lights.pointLights.size(), size_t(4)); ++i) b.point_lights[i] = *lights.pointLights[i];

        GLfloat defaultColor[] = { 0.75f, 0.75f, 0.75f, 1.0f };
        GLfloat defaultDepth = 1.f;

        // Fixme: center eye
        const ShadowData d{ shadow->get_output_texture(), lights.directionalLight->direction };
        const RenderPassData shadowPassData(0, eyes[0], d);
        if (renderShadows)
        {
            for (int c = 0; c < 4; c++)
            {
                b.cascadesPlane[c] = float4(shadow->splitPlanes[c].x, shadow->splitPlanes[c].y, 0, 0);
                b.cascadesMatrix[c] = shadow->shadowMatrices[c];
                b.cascadesNear[c] = shadow->nearPlanes[c];
                b.cascadesFar[c] = shadow->farPlanes[c];
            }
            run_shadow_pass(shadowPassData);
        }

        perScene.set_buffer_data(sizeof(b), &b, GL_STREAM_DRAW);

        renderTimer.start();
        for (int eyeIdx = 0; eyeIdx < NumEyes; ++eyeIdx)
        {
            // Update per-view uniform buffer
            uniforms::per_view v = {};
            v.view = eyes[eyeIdx].pose.inverse().matrix();
            v.viewProj = mul(eyes[eyeIdx].projectionMatrix, eyes[eyeIdx].pose.inverse().matrix());
            v.eyePos = float4(eyes[eyeIdx].pose.position, 1);
            perView.set_buffer_data(sizeof(v), &v, GL_STREAM_DRAW);

            // Update render pass data. 
            eyes[eyeIdx].viewMatrix = v.view;
            eyes[eyeIdx].viewProjMatrix = v.viewProj;

            const RenderPassData renderPassData(eyeIdx, eyes[eyeIdx], d);

            // Render into 4x multisampled fbo
            glEnable(GL_MULTISAMPLE);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, multisampleFramebuffer);
            glViewport(0, 0, renderSizePerEye.x, renderSizePerEye.y);
            glClearNamedFramebufferfv(multisampleFramebuffer, GL_COLOR, 0, &defaultColor[0]);
            glClearNamedFramebufferfv(multisampleFramebuffer, GL_DEPTH, 0, &defaultDepth);

            // Execute the forward passes
            run_skybox_pass(renderPassData);
            run_forward_pass(renderPassData);

            glDisable(GL_MULTISAMPLE);

            // Resolve multisample into per-eye textures
            glBlitNamedFramebuffer(multisampleFramebuffer, eyeTextures[eyeIdx], 0, 0, renderSizePerEye.x, renderSizePerEye.y, 0, 0, renderSizePerEye.x, renderSizePerEye.y, GL_COLOR_BUFFER_BIT, GL_LINEAR);

            // Execute the post passes after having resolved the multisample framebuffers
            run_post_pass(renderPassData);

            gl_check_error(__FILE__, __LINE__);
        }

        renderTimer.stop();

        renderSet.clear();
    }

    void gather_imgui()
    {
        ImGui::Text("Render %f", renderTimer.elapsed_ms());
        ImGui::Checkbox("Render Shadows", &renderShadows);
        ImGui::Checkbox("Render Post", &renderPost);
        ImGui::Checkbox("Render Bloom", &renderBloom);
        bloom->gather_imgui(renderBloom);
    }

    void set_eye_data(const EyeData left, const EyeData right)
    {
        eyes[0] = left;
        eyes[1] = right;
    }

    GLuint get_eye_texture(const Eye e) { return outputTextureHandles[(int)e]; }

    // A `Renderable` is a generic interface for this engine, appropriate for use with
    // the material system and all customizations (frustum culling, etc)
    void add_renderables(const std::vector<Renderable *> & set) { renderSet = set; }

    void set_lights(const LightCollection & collection) { lights = collection; }
};

#endif // end vr_renderer_hpp
  