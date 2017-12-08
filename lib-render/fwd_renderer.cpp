#include "fwd_renderer.hpp"
#include "material.hpp"
#include "math-spatial.hpp"
#include "geometry.hpp"

// Update per-object uniform buffer
void PhysicallyBasedRenderer::update_per_object_uniform_buffer(Renderable * r, const ViewData & d)
{
    uniforms::per_object object = {};
    object.modelMatrix = mul(r->get_pose().matrix(), make_scaling_matrix(r->get_scale()));
    object.modelMatrixIT = inverse(transpose(object.modelMatrix));
    object.modelViewMatrix = mul(d.viewMatrix, object.modelMatrix);
    object.receiveShadow = (float)r->get_receive_shadow();
    perObject.set_buffer_data(sizeof(object), &object, GL_STREAM_DRAW);
}

void PhysicallyBasedRenderer::add_camera(const uint32_t index, const Pose & p, const float4x4 & projectionMatrix)
{
    assert(uint32_t(index) <= settings.cameraCount);

    ViewData v;
    v.index = index;
    v.pose = p;
    v.viewMatrix = p.view_matrix();
    v.projectionMatrix = projectionMatrix;
    v.viewProjMatrix = mul(v.projectionMatrix, v.viewMatrix);
    near_far_clip_from_projection(v.projectionMatrix, v.nearClip, v.farClip);
    views[uint32_t(index)] = v;
}

void PhysicallyBasedRenderer::run_depth_prepass(const ViewData & d)
{
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
        update_per_object_uniform_buffer(obj, d);
        obj->draw();
    }
    shader.unbind();

    // Restore color mask state
    glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
}

void PhysicallyBasedRenderer::run_skybox_pass(const ViewData & d)
{
    if (!skybox) return;

    GLboolean wasDepthTestingEnabled = glIsEnabled(GL_DEPTH_TEST);
    glDisable(GL_DEPTH_TEST);

    skybox->render(d.viewProjMatrix, d.pose.position, d.farClip);

    if (wasDepthTestingEnabled) glEnable(GL_DEPTH_TEST);
}

void PhysicallyBasedRenderer::run_shadow_pass(const ViewData & d)
{
    shadow->update_cascades(d.viewMatrix,
        d.nearClip,
        d.farClip,
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

void PhysicallyBasedRenderer::run_forward_pass(std::vector<Renderable *> & renderQueueMaterial, std::vector<Renderable *> & renderQueueDefault, const ViewData & d)
{
    if (settings.useDepthPrepass)
    {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE); // depth already comes from the prepass
    }

    for (auto r : renderQueueMaterial)
    {
        update_per_object_uniform_buffer(r, d);

        Material * mat = r->get_material();
        mat->update_uniforms();
        if (auto * mr = dynamic_cast<MetallicRoughnessMaterial*>(mat)) mr->update_cascaded_shadow_array_handle(shadow->get_output_texture());
        mat->use();

        r->draw();
    }

    // We assume that objects without a valid material take care of their own shading in the `draw()` function. 
    for (auto r : renderQueueDefault)
    {
        update_per_object_uniform_buffer(r, d);
        r->draw();
    }

    if (settings.useDepthPrepass)
    {
        glDepthMask(GL_TRUE); // cleanup state
    }
}

void PhysicallyBasedRenderer::run_post_pass(const ViewData & d)
{
    GLboolean wasCullingEnabled = glIsEnabled(GL_CULL_FACE);
    GLboolean wasDepthTestingEnabled = glIsEnabled(GL_DEPTH_TEST);

    // Disable culling and depth testing for post processing
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    if (settings.bloomEnabled)
    {
        bloom->execute(eyeTextures[d.index]);
        glBlitNamedFramebuffer(bloom->get_output_framebuffer(), eyeFramebuffers[d.index], 0, 0,
            settings.renderSize.x, settings.renderSize.y, 0, 0,
            settings.renderSize.x, settings.renderSize.y, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    }

    if (wasCullingEnabled) glEnable(GL_CULL_FACE);
    if (wasDepthTestingEnabled) glEnable(GL_DEPTH_TEST);
}

PhysicallyBasedRenderer::PhysicallyBasedRenderer(const RendererSettings settings) : settings(settings)
{
    assert(settings.renderSize.x > 0 && settings.renderSize.y > 0);
    assert(settings.cameraCount >= 1);

    views.resize(settings.cameraCount);
    eyeFramebuffers.resize(settings.cameraCount);
    eyeTextures.resize(settings.cameraCount);
    eyeDepthTextures.resize(settings.cameraCount);

    // Generate multisample render buffers for color and depth, attach to multi-sampled framebuffer target
    glNamedRenderbufferStorageMultisampleEXT(multisampleRenderbuffers[0], settings.msaaSamples, GL_RGBA8, settings.renderSize.x, settings.renderSize.y);
    glNamedFramebufferRenderbufferEXT(multisampleFramebuffer, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, multisampleRenderbuffers[0]);
    glNamedRenderbufferStorageMultisampleEXT(multisampleRenderbuffers[1], settings.msaaSamples, GL_DEPTH_COMPONENT, settings.renderSize.x, settings.renderSize.y);
    glNamedFramebufferRenderbufferEXT(multisampleFramebuffer, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, multisampleRenderbuffers[1]);

    multisampleFramebuffer.check_complete();

    // Generate textures and framebuffers for `settings.cameraCount`
    for (int camIdx = 0; camIdx < settings.cameraCount; ++camIdx)
    {
        eyeTextures[camIdx].setup(settings.renderSize.x, settings.renderSize.y, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, nullptr, false);
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

PhysicallyBasedRenderer::~PhysicallyBasedRenderer()
{
    timer.stop();
}

void PhysicallyBasedRenderer::update()
{
    // ... 
}

void PhysicallyBasedRenderer::render_frame()
{
    cpuProfiler.begin("renderloop");

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
    for (int i = 0; i < (int) std::min(pointLights.size(), size_t(uniforms::MAX_POINT_LIGHTS)); ++i) b.point_lights[i] = pointLights[i];

    GLfloat defaultColor[] = { 1.0f, 0.0f, 0.f, 1.0f };
    GLfloat defaultDepth = 1.f;

    ViewData shadowAndCullingView = views[0];
    if (settings.cameraCount == 2)
    {
        cpuProfiler.begin("center-view");
        // Take the mid-point between the eyes
        shadowAndCullingView.pose = Pose(views[0].pose.orientation, (views[0].pose.position + views[1].pose.position) * 0.5f);

        // Compute the interocular distance
        const float3 interocularDistance = views[1].pose.position - views[0].pose.position;

        // Generate the superfrustum projection matrix and the value we need to move the midpoint in Z
        float3 centerOffsetZ;
        compute_center_view(views[0].projectionMatrix, views[1].projectionMatrix, interocularDistance.x, shadowAndCullingView.projectionMatrix, centerOffsetZ);

        // Regenerate the view matrix and near/far clip planes
        shadowAndCullingView.viewMatrix = inverse(mul(shadowAndCullingView.pose.matrix(), make_translation_matrix(centerOffsetZ)));
        near_far_clip_from_projection(shadowAndCullingView.projectionMatrix, shadowAndCullingView.nearClip, shadowAndCullingView.farClip);
        cpuProfiler.end("center-view");
    }

    if (settings.shadowsEnabled)
    {
        gpuProfiler.begin("shadowpass");
        run_shadow_pass(shadowAndCullingView);
        gpuProfiler.end("shadowpass");

        for (int c = 0; c < uniforms::NUM_CASCADES; c++)
        {
            b.cascadesPlane[c] = float4(shadow->splitPlanes[c].x, shadow->splitPlanes[c].y, 0, 0);
            b.cascadesMatrix[c] = shadow->shadowMatrices[c];
            b.cascadesNear[c] = shadow->nearPlanes[c];
            b.cascadesFar[c] = shadow->farPlanes[c];
        }
    }

    // Per-scene can be uploaded now that the shadow pass has completed
    perScene.set_buffer_data(sizeof(b), &b, GL_STREAM_DRAW);

    // We follow the sorting strategy outlined here: http://realtimecollisiondetection.net/blog/?p=86
    auto materialSortFunc = [shadowAndCullingView](Renderable * lhs, Renderable * rhs)
    {
        const float lDist = distance(shadowAndCullingView.pose.position, lhs->get_pose().position);
        const float rDist = distance(shadowAndCullingView.pose.position, rhs->get_pose().position);

        // Sort by material (expensive shader state change)
        auto lid = lhs->get_material()->id();
        auto rid = rhs->get_material()->id();
        if (lid != rid) return lid > rid;

        // Otherwise sort by distance
        return lDist < rDist;
    };

    auto distanceSortFunc = [shadowAndCullingView](Renderable * lhs, Renderable * rhs)
    {
        const float lDist = distance(shadowAndCullingView.pose.position, lhs->get_pose().position);
        const float rDist = distance(shadowAndCullingView.pose.position, rhs->get_pose().position);
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
        v.view = views[camIdx].pose.inverse().matrix();
        v.viewProj = mul(views[camIdx].projectionMatrix, views[camIdx].pose.inverse().matrix());
        v.eyePos = float4(views[camIdx].pose.position, 1);
        perView.set_buffer_data(sizeof(v), &v, GL_STREAM_DRAW);

        // Update render pass data
        views[camIdx].viewMatrix = v.view;
        views[camIdx].viewProjMatrix = v.viewProj;

        // Render into multisampled fbo
        glEnable(GL_MULTISAMPLE);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, multisampleFramebuffer);
        glViewport(0, 0, settings.renderSize.x, settings.renderSize.y);
        glClearNamedFramebufferfv(multisampleFramebuffer, GL_COLOR, 0, &defaultColor[0]);
        glClearNamedFramebufferfv(multisampleFramebuffer, GL_DEPTH, 0, &defaultDepth);

        // Execute the forward passes
        if (settings.useDepthPrepass)
        {
            gpuProfiler.begin("depth-prepass");
            run_depth_prepass(views[camIdx]);
            gpuProfiler.end("depth-prepass");
        }

        gpuProfiler.begin("forward pass");
        run_skybox_pass(views[camIdx]);
        run_forward_pass(materialRenderList, defaultRenderList, views[camIdx]);
        gpuProfiler.end("forward pass");

        glDisable(GL_MULTISAMPLE);

        // Resolve multisample into per-view framebuffer
        {
            gpuProfiler.begin("blit");

            // blit color 
            glBlitNamedFramebuffer(multisampleFramebuffer, eyeFramebuffers[camIdx],
                0, 0, settings.renderSize.x, settings.renderSize.y, 0, 0,
                settings.renderSize.x, settings.renderSize.y, GL_COLOR_BUFFER_BIT, GL_LINEAR);

            // blit depth
            glBlitNamedFramebuffer(multisampleFramebuffer, eyeFramebuffers[camIdx],
                0, 0, settings.renderSize.x, settings.renderSize.y, 0, 0,
                settings.renderSize.x, settings.renderSize.y, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

            gpuProfiler.end("blit");
        }

        gl_check_error(__FILE__, __LINE__);
    }

    // Execute the post passes after having resolved the multisample framebuffers
    {
        gpuProfiler.begin("postprocess");
        for (int camIdx = 0; camIdx < settings.cameraCount; ++camIdx)
        {
            run_post_pass(views[camIdx]);
        }
        gpuProfiler.end("postprocess");
    }

    glDisable(GL_FRAMEBUFFER_SRGB);

    cpuProfiler.end("renderloop");

    renderSet.clear();
    pointLights.clear();

    gl_check_error(__FILE__, __LINE__);
}