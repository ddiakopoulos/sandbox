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
#include "circular_buffer.hpp"
#include "debug_line_renderer.hpp" 

#include "gl-scene.hpp"
#include "gl-camera.hpp"
#include "gl-async-gpu-timer.hpp"
#include "gl-procedural-sky.hpp"
#include "gl-imgui.hpp"

#include "bloom_pass.hpp"
#include "shadow_pass.hpp"

using namespace avl;

struct CameraData
{
    Pose pose;
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
    float4x4 viewProjMatrix;
};

struct RenderLightingData
{
    uniforms::directional_light * directionalLight;
    std::vector<uniforms::point_light *> pointLights;
};

struct RenderPassData
{
    const uint32_t eye;
    const CameraData data;
    RenderPassData(const uint32_t eye, const CameraData & data) : eye(eye), data(data) {}
};

template<uint32_t NumEyes>
class PhysicallyBasedRenderer
{
    std::vector<Renderable *> renderSet;
    RenderLightingData lights;

    float2 renderSizePerEye;

    GlGpuTimer forwardTimer;
    GlGpuTimer shadowTimer;
    GlGpuTimer postTimer;

    CircularBuffer<float> forwardAverage = { 3 };
    CircularBuffer<float> shadowAverage = { 3 };
    CircularBuffer<float> postAverage = { 3 };
    CircularBuffer<float> frameAverage = { 3 };

    SimpleTimer timer;

    GlBuffer perScene;
    GlBuffer perView;
    GlBuffer perObject;

    CameraData cameras[NumEyes];

    GlFramebuffer eyeFramebuffers[NumEyes];
    GlTexture2D eyeTextures[NumEyes];
    GlRenderbuffer multisampleRenderbuffers[NumEyes];
    GlFramebuffer multisampleFramebuffer;

    GLuint outputTextureHandles[NumEyes];

    bool renderPost{ true };
    bool renderShadows{ true };
    bool renderBloom{ true };

    ProceduralSky * skybox{ nullptr };

    void run_skybox_pass(const RenderPassData & d)
    {   
        if (!skybox) return;
        glDisable(GL_DEPTH_TEST);
        skybox->render(d.data.viewProjMatrix, d.data.pose.position, near_far_clip_from_projection(d.data.projectionMatrix).y);
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
            lights.directionalLight->direction);

        shadow->pre_draw();

        for (Renderable * obj : renderSet)
        {
            if (obj->get_cast_shadow())
            {
                float4x4 modelMatrix = mul(obj->get_pose().matrix(), make_scaling_matrix(obj->get_scale()));
                shadow->program.get().uniform("u_modelShadowMatrix", modelMatrix);
                obj->draw();
            }
        }

        shadow->post_draw();
    }

    void run_forward_pass(const RenderPassData & d)
    {
        glEnable(GL_DEPTH_TEST);

        // todo - this is done per-eye but should be done per frame instead
        auto renderSortFunc = [&d](Renderable * lhs, Renderable * rhs)
        {
            auto lid = lhs->get_material()->id();
            auto rid = rhs->get_material()->id();

            float3 cameraWorldspace = d.data.pose.position;
            float lDist = distance(cameraWorldspace, lhs->get_pose().position);
            float rDist = distance(cameraWorldspace, rhs->get_pose().position);

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

            // Update per-object uniform buffer
            uniforms::per_object object = {};
            object.modelMatrix = mul(top->get_pose().matrix(), make_scaling_matrix(top->get_scale()));
            object.modelMatrixIT = inverse(transpose(object.modelMatrix));
            object.modelViewMatrix = mul(d.data.viewMatrix, object.modelMatrix);
            object.receiveShadow = top->get_receive_shadow();
            perObject.set_buffer_data(sizeof(object), &object, GL_STREAM_DRAW);

            if (auto * mr = dynamic_cast<MetallicRoughnessMaterial*>(mat))
            {
                mr->update_cascaded_shadow_array_handle(shadow->get_output_texture());
            }

            mat->use();
            top->draw();

            renderQueue.pop();
        }

        for (int eyeIndex = 0; eyeIndex < NumEyes; ++eyeIndex)
        {
            outputTextureHandles[eyeIndex] = eyeTextures[eyeIndex];
        }
    }

    void run_post_pass(const RenderPassData & d)
    {
        if (!renderPost) return;

        GLboolean wasCullingEnabled = glIsEnabled(GL_CULL_FACE);
        GLboolean wasDepthTestingEnabled = glIsEnabled(GL_DEPTH_TEST);

        // Disable culling and depth testing for post processing
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);

        if (renderBloom)
        {
            run_bloom_pass(d);
        }

        if (wasCullingEnabled) glEnable(GL_CULL_FACE);
        if (wasDepthTestingEnabled) glEnable(GL_DEPTH_TEST);
    }

    void run_bloom_pass(const RenderPassData & d)
    {
        bloom->execute(eyeTextures[d.eye]);
        glBlitNamedFramebuffer(bloom->get_output_texture(), eyeTextures[d.eye], 0, 0, renderSizePerEye.x, renderSizePerEye.y, 0, 0, renderSizePerEye.x, renderSizePerEye.y, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    }

public:

    std::unique_ptr<BloomPass> bloom;
    std::unique_ptr<StableCascadedShadowPass> shadow;

    PhysicallyBasedRenderer(const float2 render_target_size) : renderSizePerEye(render_target_size)
    {
        assert(renderSizePerEye.x >= 0 && renderSizePerEye.y >= 0);
        assert(NumEyes >= 1);

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

        shadow.reset(new StableCascadedShadowPass());
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

        glEnable(GL_FRAMEBUFFER_SRGB);

        glBindBufferBase(GL_UNIFORM_BUFFER, uniforms::per_scene::binding, perScene);
        glBindBufferBase(GL_UNIFORM_BUFFER, uniforms::per_view::binding, perView);
        glBindBufferBase(GL_UNIFORM_BUFFER, uniforms::per_object::binding, perObject);

        // Update per-scene uniform buffer
        uniforms::per_scene b = {};
        b.time = timer.milliseconds().count();
        b.resolution = renderSizePerEye;
        b.invResolution = 1.f / b.resolution;
        b.activePointLights = lights.pointLights.size();

        b.directional_light.color = lights.directionalLight->color;
        b.directional_light.direction = lights.directionalLight->direction;
        b.directional_light.amount = lights.directionalLight->amount;
        for (int i = 0; i < (int)std::min(lights.pointLights.size(), size_t(uniforms::MAX_POINT_LIGHTS)); ++i) b.point_lights[i] = *lights.pointLights[i];

        GLfloat defaultColor[] = { 0.75f, 0.75f, 0.75f, 1.0f };
        GLfloat defaultDepth = 1.f;
    
        shadowTimer.start();

        if (renderShadows)
        {
            // Default to the first camera
            CameraData shadowCamera = cameras[0];

            // In VR, we create a virtual camera in between both eyes.
            if (NumEyes == 2)
            {
                // Average the positions and re-generate the relevant matrices
                const float3 centerPosition = (cameras[0].pose.position + cameras[1].pose.position) * 0.5f;
                shadowCamera.pose.position = centerPosition;
                shadowCamera.viewMatrix = make_view_matrix_from_pose(shadowCamera.pose);
                shadowCamera.viewProjMatrix = mul(shadowCamera.projectionMatrix, shadowCamera.viewMatrix);
            }

            run_shadow_pass({ 0, shadowCamera });

            for (int c = 0; c < uniforms::NUM_CASCADES; c++)
            {
                b.cascadesPlane[c] = float4(shadow->splitPlanes[c].x, shadow->splitPlanes[c].y, 0, 0);
                b.cascadesMatrix[c] = shadow->shadowMatrices[c];
                b.cascadesNear[c] = shadow->nearPlanes[c];
                b.cascadesFar[c] = shadow->farPlanes[c];
            }
        }

        shadowTimer.stop();

        forwardTimer.start();

        // Per-scene can be uploaded now that the shadow pass has completed
        perScene.set_buffer_data(sizeof(b), &b, GL_STREAM_DRAW);

        for (int eyeIdx = 0; eyeIdx < NumEyes; ++eyeIdx)
        {
            // Update per-view uniform buffer
            uniforms::per_view v = {};
            v.view = cameras[eyeIdx].pose.inverse().matrix();
            v.viewProj = mul(cameras[eyeIdx].projectionMatrix, cameras[eyeIdx].pose.inverse().matrix());
            v.eyePos = float4(cameras[eyeIdx].pose.position, 1);
            perView.set_buffer_data(sizeof(v), &v, GL_STREAM_DRAW);

            // Update render pass data. 
            cameras[eyeIdx].viewMatrix = v.view;
            cameras[eyeIdx].viewProjMatrix = v.viewProj;

            const RenderPassData renderPassData(eyeIdx, cameras[eyeIdx]);

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

            gl_check_error(__FILE__, __LINE__);
        }

        forwardTimer.stop();

        postTimer.start();

        // Execute the post passes after having resolved the multisample framebuffers
        for (int eyeIdx = 0; eyeIdx < NumEyes; ++eyeIdx)
        {
            const RenderPassData renderPassData(eyeIdx, cameras[eyeIdx]);
            run_post_pass(renderPassData);
        }

        postTimer.stop();

        glDisable(GL_FRAMEBUFFER_SRGB);

        renderSet.clear();
    }

    void gather_imgui()
    {
        float shadowMs = shadowTimer.elapsed_ms();
        float forwardMs = forwardTimer.elapsed_ms();
        float postMs = postTimer.elapsed_ms();

        forwardAverage.put(forwardMs);
        shadowAverage.put(shadowMs);
        postAverage.put(postMs);
        frameAverage.put(shadowMs + forwardMs + postMs);

        ImGui::Text("Shadow  %f ms", compute_mean(shadowAverage));
        ImGui::Text("Forward %f ms", compute_mean(forwardAverage));
        ImGui::Text("Post    %f ms", compute_mean(postAverage));
        ImGui::Text("Frame   %f ms", compute_mean(frameAverage));

        if (ImGui::CollapsingHeader("Cascaded Shadow Mapping"))
        {
            ImGui::Checkbox("Render Shadows", &renderShadows);
            if (renderShadows) shadow->gather_imgui(renderShadows);
        }

        if (ImGui::CollapsingHeader("Post Processing"))
        {
            ImGui::Checkbox("Render Post", &renderPost);
            if (renderPost)
            {
                if (ImGui::CollapsingHeader("Bloom"))
                {
                    ImGui::Checkbox("Render Bloom", &renderBloom);
                    if (renderBloom) bloom->gather_imgui(renderBloom);
                }
            }
        }
    }

    void add_camera(const uint32_t idx, const CameraData data)
    {
        assert(idx <= NumEyes);
        cameras[idx] = data;
    }

    GLuint get_output_texture(const uint32_t idx) const
    { 
        assert(idx <= NumEyes);
        return outputTextureHandles[idx]; 
    }

    void set_procedural_sky(ProceduralSky * sky)
    {
        skybox = sky;
    }

    ProceduralSky * get_procedural_sky() const
    {
        if (skybox) return skybox;
        else return nullptr;
    }

    StableCascadedShadowPass * get_shadow_pass() const
    {
        if (shadow) return shadow.get();
    }

    void add_objects(const std::vector<Renderable *> & set) { renderSet = set; }

    void add_lights(const RenderLightingData & collection) { lights = collection; }
};

#endif // end vr_renderer_hpp
  