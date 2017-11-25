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
#include "human_time.hpp"

#include "gl-camera.hpp"
#include "gl-async-gpu-timer.hpp"
#include "gl-procedural-sky.hpp"
#include "gl-imgui.hpp"

#include "scene.hpp"
#include "bloom_pass.hpp"
#include "shadow_pass.hpp"

using namespace avl;

struct RendererSettings
{
    uint32_t cameraCount = 1;
    uint32_t msaaSamples = 4;
    float2 renderSize;
    bool performanceProfiling = true;
    bool useDepthPrepass = true;
};

struct CameraData
{
    uint32_t index;
    Pose pose;
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
    float4x4 viewProjMatrix;
};

class PhysicallyBasedRenderer
{
    RendererSettings settings;

    GlGpuTimer earlyZTimer;
    GlGpuTimer forwardTimer;
    GlGpuTimer shadowTimer;
    GlGpuTimer postTimer;

    SimpleTimer timer;

    GlBuffer perScene;
    GlBuffer perView;
    GlBuffer perObject;

    std::vector<CameraData> cameras;

    // MSAA 
    GlRenderbuffer multisampleRenderbuffers[2];
    GlFramebuffer multisampleFramebuffer;

    // Non-MSAA Targets
    std::vector<GlFramebuffer> eyeFramebuffers;
    std::vector<GlTexture2D> eyeTextures;
    std::vector<GlTexture2D> eyeDepthTextures;

    std::vector<Renderable *> renderSet;
    std::vector<uniforms::point_light *> pointLights;

    uniforms::directional_light sunlight;
    ProceduralSky * skybox{ nullptr };

    std::unique_ptr<BloomPass> bloom;
    std::unique_ptr<StableCascadedShadowPass> shadow;

    GlShaderHandle earlyZPass = { "depth-prepass" };

    // Update per-object uniform buffer
    void update_per_object(Renderable * top, const CameraData & d)
    {
        uniforms::per_object object = {};
        object.modelMatrix = mul(top->get_pose().matrix(), make_scaling_matrix(top->get_scale()));
        object.modelMatrixIT = inverse(transpose(object.modelMatrix));
        object.modelViewMatrix = mul(d.viewMatrix, object.modelMatrix);
        object.receiveShadow = (float)top->get_receive_shadow();
        perObject.set_buffer_data(sizeof(object), &object, GL_STREAM_DRAW);
    };

    void run_depth_prepass(const CameraData & d)
    {
        earlyZTimer.start();

        GLboolean colorMask[4];
        glGetBooleanv(GL_COLOR_WRITEMASK, &colorMask[0]);

        glEnable(GL_DEPTH_TEST);    // Enable depth testing
        glDepthFunc(GL_LESS);       // Nearest pixel
        glDepthMask(GL_TRUE);       // Need depth mask on
        glColorMask(0, 0, 0, 0);    // Do not write any color

        auto & shader = earlyZPass.get();
        shader.bind();

        for (auto obj : renderSet)
        {
            update_per_object(obj, d);
            obj->draw();
        }

        // Restore color mask state
        glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);

        shader.unbind();

        earlyZTimer.stop();
    }

    void run_skybox_pass(const CameraData & d)
    {   
        if (!skybox) return;

        GLboolean wasDepthTestingEnabled = glIsEnabled(GL_DEPTH_TEST);
        glDisable(GL_DEPTH_TEST);

        skybox->render(d.viewProjMatrix, d.pose.position, near_far_clip_from_projection(d.projectionMatrix).y);

        if (wasDepthTestingEnabled) glEnable(GL_DEPTH_TEST);
    }

    void run_shadow_pass(const CameraData & d)
    {
        const float2 nearFarClip = near_far_clip_from_projection(d.projectionMatrix);

        shadow->update_cascades(make_view_matrix_from_pose(d.pose),
            nearFarClip.x,
            nearFarClip.y,
            aspect_from_projection(d.projectionMatrix),
            vfov_from_projection(d.projectionMatrix),
            sunlight.direction);

        shadow->pre_draw();

        gl_check_error(__FILE__, __LINE__);

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

        gl_check_error(__FILE__, __LINE__);
    }

    void run_forward_pass(std::vector<Renderable *> & renderQueueMaterial, std::vector<Renderable *> & renderQueueDefault, const CameraData & d)
    {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE); // depth already comes from the prepass

        for (auto r : renderQueueMaterial)
        {
            update_per_object(r, d);

            Material * mat = r->get_material();
            mat->update_uniforms();
            if (auto * mr = dynamic_cast<MetallicRoughnessMaterial*>(mat)) mr->update_cascaded_shadow_array_handle(shadow->get_output_texture());
            mat->use();

            r->draw();
        }

        // We assume that objects without a valid material take care of their own shading in the `draw()` function. 
        for (auto r : renderQueueDefault)
        {
            update_per_object(r, d);
            r->draw();
        }

        glDepthMask(GL_TRUE); // cleanup state
    }

    void run_post_pass(const CameraData & d)
    {
        GLboolean wasCullingEnabled = glIsEnabled(GL_CULL_FACE);
        GLboolean wasDepthTestingEnabled = glIsEnabled(GL_DEPTH_TEST);

        // Disable culling and depth testing for post processing
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);

        run_bloom_pass(d);

        if (wasCullingEnabled) glEnable(GL_CULL_FACE);
        if (wasDepthTestingEnabled) glEnable(GL_DEPTH_TEST);
    }

    void run_bloom_pass(const CameraData & d)
    {
        bloom->execute(eyeTextures[d.index]);
        glBlitNamedFramebuffer(bloom->get_output_texture(), eyeTextures[d.index], 0, 0,
            settings.renderSize.x, settings.renderSize.y, 0, 0, 
            settings.renderSize.x, settings.renderSize.y, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    }

public:

    PhysicallyBasedRenderer(const RendererSettings & settings) : settings(settings)
    {
        assert(settings.renderSize.x >= 0 && settings.renderSize.y >= 0);
        assert(settings.cameraCount >= 1);

        cameras.resize(settings.cameraCount);
        eyeFramebuffers.resize(settings.cameraCount);
        eyeTextures.resize(settings.cameraCount);
        eyeDepthTextures.resize(settings.cameraCount);

        // Generate multisample render buffers for color and depth, attach to multi-sampled framebuffer target
        glNamedRenderbufferStorageMultisampleEXT(multisampleRenderbuffers[0], 4, GL_RGBA8, settings.renderSize.x, settings.renderSize.y);
        glNamedFramebufferRenderbufferEXT(multisampleFramebuffer, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, multisampleRenderbuffers[0]);
        glNamedRenderbufferStorageMultisampleEXT(multisampleRenderbuffers[1], 4, GL_DEPTH_COMPONENT, settings.renderSize.x, settings.renderSize.y);
        glNamedFramebufferRenderbufferEXT(multisampleFramebuffer, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, multisampleRenderbuffers[1]);

        multisampleFramebuffer.check_complete();

        // Generate textures and framebuffers for `settings.cameraCount`
        for (int camIdx = 0; camIdx < settings.cameraCount; ++camIdx)
        {
            glTextureImage2DEXT(eyeTextures[camIdx], GL_TEXTURE_2D, 0, GL_RGBA8, settings.renderSize.x, settings.renderSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTextureParameteriEXT(eyeTextures[camIdx], GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTextureParameteriEXT(eyeTextures[camIdx], GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTextureParameteriEXT(eyeTextures[camIdx], GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTextureParameteriEXT(eyeTextures[camIdx], GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTextureParameteriEXT(eyeTextures[camIdx], GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

            // Depth tex
            eyeDepthTextures[camIdx].setup(settings.renderSize.x, settings.renderSize.y, GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            glNamedFramebufferTexture2DEXT(eyeFramebuffers[camIdx], GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, eyeTextures[camIdx], 0);
            glNamedFramebufferTexture2DEXT(eyeFramebuffers[camIdx], GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, eyeDepthTextures[camIdx], 0);

            eyeFramebuffers[camIdx].check_complete();
        }

        shadow.reset(new StableCascadedShadowPass());
        bloom.reset(new BloomPass(settings.renderSize));

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
        b.time = timer.milliseconds().count() / 1000.f; // millisecond resolution expressed as seconds
        b.resolution = settings.renderSize;
        b.invResolution = 1.f / b.resolution;
        b.activePointLights = pointLights.size();

        b.directional_light.color = sunlight.color;
        b.directional_light.direction = sunlight.direction;
        b.directional_light.amount = sunlight.amount;
        for (int i = 0; i < (int)std::min(pointLights.size(), size_t(uniforms::MAX_POINT_LIGHTS)); ++i) b.point_lights[i] = *pointLights[i];

        GLfloat defaultColor[] = { 1.0f, 0.0f, 0.f, 1.0f };
        GLfloat defaultDepth = 1.f;
        
        shadowTimer.start();

        // In VR, we create a virtual camera in between both eyes.
        // todo - this is somewhat wrong since we need to actually create a superfrustum
        // which is max(left, right)
        float3 cameraWorldspace;
        if (settings.cameraCount == 2) cameraWorldspace = (cameras[0].pose.position + cameras[1].pose.position) * 0.5f;
        else cameraWorldspace = cameras[0].pose.position;

        if (shadow->enabled)
        {
            // Default to the first camera
            CameraData shadowCamera = cameras[0];

            // regenerate the relevant matrices because we've already changed camera position
            if (settings.cameraCount == 2)
            {
                // Average the positions and 
                shadowCamera.pose.position = cameraWorldspace;
                shadowCamera.viewMatrix = make_view_matrix_from_pose(shadowCamera.pose);
                shadowCamera.viewProjMatrix = mul(shadowCamera.projectionMatrix, shadowCamera.viewMatrix);
            }

            run_shadow_pass(shadowCamera);

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

        // Follows sorting strategy outlined here: 
        // http://realtimecollisiondetection.net/blog/?p=86

        auto materialSortFunc = [cameraWorldspace](Renderable * lhs, Renderable * rhs)
        {
            const float lDist = distance(cameraWorldspace, lhs->get_pose().position);
            const float rDist = distance(cameraWorldspace, rhs->get_pose().position);

            // Sort by material (expensive shader state change)
            auto lid = lhs->get_material()->id();
            auto rid = rhs->get_material()->id();
            if (lid != rid) return lid > rid;

            // Otherwise sort by distance
            return lDist < rDist;
        };

        auto distanceSortFunc = [cameraWorldspace](Renderable * lhs, Renderable * rhs)
        {
            const float lDist = distance(cameraWorldspace, lhs->get_pose().position);
            const float rDist = distance(cameraWorldspace, rhs->get_pose().position);
            return lDist < rDist;
        };

        std::priority_queue<Renderable *, std::vector<Renderable*>, decltype(materialSortFunc)> renderQueueMaterial(materialSortFunc);
        std::priority_queue<Renderable *, std::vector<Renderable*>, decltype(distanceSortFunc)> renderQueueDefault(distanceSortFunc);

        for (auto obj : renderSet)
        {
            // Can't sort by material if the renderable doesn't *have* a material; bucket all other objects 
            if (obj->get_material() != nullptr) renderQueueMaterial.push(obj);
            else renderQueueDefault.push(obj);
        }

        // Resolve render queues into flat lists
        std::vector<Renderable *> materialRenderList;
        while (!renderQueueMaterial.empty())
        {
            Renderable * top = renderQueueMaterial.top();
            renderQueueMaterial.pop();
            materialRenderList.push_back(top);
        }

        std::vector<Renderable *> defaultRenderList;
        while (!renderQueueDefault.empty())
        {
            Renderable * top = renderQueueDefault.top();
            renderQueueDefault.pop();
            defaultRenderList.push_back(top);
        }

        for (int camIdx = 0; camIdx < settings.cameraCount; ++camIdx)
        {
            // Update per-view uniform buffer
            uniforms::per_view v = {};
            v.view = cameras[camIdx].pose.inverse().matrix();
            v.viewProj = mul(cameras[camIdx].projectionMatrix, cameras[camIdx].pose.inverse().matrix());
            v.eyePos = float4(cameras[camIdx].pose.position, 1);
            perView.set_buffer_data(sizeof(v), &v, GL_STREAM_DRAW);

            // Update render pass data
            cameras[camIdx].viewMatrix = v.view;
            cameras[camIdx].viewProjMatrix = v.viewProj;

            // Render into multisampled fbo
            glEnable(GL_MULTISAMPLE);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, multisampleFramebuffer);
            glViewport(0, 0, settings.renderSize.x, settings.renderSize.y);
            glClearNamedFramebufferfv(multisampleFramebuffer, GL_COLOR, 0, &defaultColor[0]);
            glClearNamedFramebufferfv(multisampleFramebuffer, GL_DEPTH, 0, &defaultDepth);

            // Execute the forward passes
            run_depth_prepass(cameras[camIdx]);
            run_skybox_pass(cameras[camIdx]);
            run_forward_pass(materialRenderList, defaultRenderList, cameras[camIdx]);

            glDisable(GL_MULTISAMPLE);

            // Resolve multisample into per-eye framebuffers
            {
                // blit color 
                glBlitNamedFramebuffer(multisampleFramebuffer, eyeFramebuffers[camIdx],
                    0, 0, settings.renderSize.x, settings.renderSize.y, 0, 0,
                    settings.renderSize.x, settings.renderSize.y, GL_COLOR_BUFFER_BIT, GL_LINEAR);

                // blit depth
                glBlitNamedFramebuffer(multisampleFramebuffer, eyeFramebuffers[camIdx],
                    0, 0, settings.renderSize.x, settings.renderSize.y, 0, 0,
                    settings.renderSize.x, settings.renderSize.y, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
            }

            gl_check_error(__FILE__, __LINE__);
        }

        forwardTimer.stop();

        // Execute the post passes after having resolved the multisample framebuffers
        {
            postTimer.start();
            for (int camIdx = 0; camIdx < settings.cameraCount; ++camIdx)
            {
                run_post_pass(cameras[camIdx]);
            }
            postTimer.stop();
        }

        glDisable(GL_FRAMEBUFFER_SRGB);

        renderSet.clear();
        pointLights.clear();

        // Compute frame GPU performance timing info
        {
            const float shadowMs = shadowTimer.elapsed_ms();
            const float earlyZMs = earlyZTimer.elapsed_ms();
            const float forwardMs = forwardTimer.elapsed_ms();
            const float postMs = postTimer.elapsed_ms();
            earlyZAverage.put(earlyZMs);
            forwardAverage.put(forwardMs);
            shadowAverage.put(shadowMs);
            postAverage.put(postMs);
            frameAverage.put(earlyZMs + shadowMs + forwardMs + postMs);
        }

        gl_check_error(__FILE__, __LINE__);
    }

    void add_camera(const CameraData & data)
    {
        assert(data.index <= settings.cameraCount);
        cameras[data.index] = data;
    }

    GLuint get_output_texture(const uint32_t idx) const
    { 
        assert(idx <= settings.cameraCount);
        return eyeTextures[idx]; 
    }

    GLuint get_output_texture_depth(const uint32_t idx) const
    {
        assert(idx <= settings.cameraCount);
        return eyeDepthTextures[idx];
    }

    void set_procedural_sky(ProceduralSky * sky)
    {
        skybox = sky;

        sunlight.direction = sky->get_sun_direction();
        sunlight.color = float3(1.f, 1.0f, 1.0f);
        sunlight.amount = 1.0f;
    }
   
    uniforms::directional_light get_sunlight() const
    {
        return sunlight;
    }

    void set_sunlight(uniforms::directional_light sun)
    {
        sunlight = sun;
    }

    ProceduralSky * get_procedural_sky() const
    {
        if (skybox) return skybox;
        else return nullptr;
    }

    StableCascadedShadowPass * get_shadow_pass() const { if (shadow) return shadow.get(); }
    BloomPass * get_bloom_pass() const { if (bloom) return bloom.get(); }

    void add_objects(const std::vector<Renderable *> & set) 
    { 
        renderSet = set; 
    }

    void add_light(uniforms::point_light * light) 
    { 
        pointLights.push_back(light);
    }

    CircularBuffer<float> earlyZAverage = { 3 };
    CircularBuffer<float> forwardAverage = { 3 };
    CircularBuffer<float> shadowAverage = { 3 };
    CircularBuffer<float> postAverage = { 3 };
    CircularBuffer<float> frameAverage = { 3 };
};

#endif // end vr_renderer_hpp
  