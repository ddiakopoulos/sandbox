#include "vr_renderer.hpp"
#include "material.hpp"
#include "avl_imgui.hpp"
#include <queue>

float vfov_from_projection(const float4x4 & p)
{
    float t = p[1][1];
    float fov = std::atan((1.0f / t)) * 2.0f;
    return fov;
}

float aspect_from_projection(const float4x4 & p)
{
    float t = p[0][0];
    float aspect = 1.0f / (t * (1.0f / p[1][1]));
    return aspect;
}

float2 near_far_clip_from_projection(const float4x4 & p)
{
    float C = p[2][2];
    float D = p[3][2];
    return{ 2 * (D / (C - 1.0f)), D / (C + 1.0f) };
}

VR_Renderer::VR_Renderer(float2 renderSizePerEye) : renderSizePerEye(renderSizePerEye)
{
    // Generate multisample render buffers for color and depth
    glNamedRenderbufferStorageMultisampleEXT(multisampleRenderbuffers[0], 4, GL_RGBA8, renderSizePerEye.x, renderSizePerEye.y);
    glNamedRenderbufferStorageMultisampleEXT(multisampleRenderbuffers[1], 4, GL_DEPTH_COMPONENT, renderSizePerEye.x, renderSizePerEye.y);

    // Generate a framebuffer for multisample rendering
    glNamedFramebufferRenderbufferEXT(multisampleFramebuffer, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, multisampleRenderbuffers[0]);
    glNamedFramebufferRenderbufferEXT(multisampleFramebuffer, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, multisampleRenderbuffers[1]);
    if (glCheckNamedFramebufferStatusEXT(multisampleFramebuffer, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) throw std::runtime_error("Framebuffer incomplete!");

    // Still can't figure out what's going on here
    //GlTexture2D IntermediateTexHack;
    //std::cout << "HackTexId: " << IntermediateTexHack << std::endl;
      
    std::cout << "Left Texture: "  <<  eyeTextures[0] << std::endl;
    std::cout << "Right Texture: " <<  eyeTextures[1] << std::endl;
    std::cout << "Render Size per Eye: " << renderSizePerEye << std::endl;

    // Generate textures and framebuffers for the left and right eye images
    for (int eye : { 0, 1 })
    {
        glTextureImage2DEXT(eyeTextures[eye], GL_TEXTURE_2D, 0, GL_RGBA8, renderSizePerEye.x, renderSizePerEye.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTextureParameteriEXT(eyeTextures[eye], GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteriEXT(eyeTextures[eye], GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteriEXT(eyeTextures[eye], GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteriEXT(eyeTextures[eye], GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteriEXT(eyeTextures[eye], GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glNamedFramebufferTexture2DEXT(eyeFramebuffers[eye], GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, eyeTextures[eye], 0);
        eyeFramebuffers[eye].check_complete();
    }

    skybox.reset(new SkyboxPass());
    bloom.reset(new BloomPass(renderSizePerEye));
    shadow.reset(new ShadowPass());

    timer.start();
}

VR_Renderer::~VR_Renderer()
{
    timer.stop();
}

void VR_Renderer::set_eye_data(const EyeData left, const EyeData right)
{
    eyes[0] = left;
    eyes[1] = right;
}

void VR_Renderer::run_skybox_pass(const RenderPassData & d)
{
    glDisable(GL_DEPTH_TEST);
    skybox->execute(d.data.pose.position, d.data.viewProjMatrix, near_far_clip_from_projection(d.data.projectionMatrix).y);
    glDisable(GL_DEPTH_TEST);
}

void VR_Renderer::run_forward_pass(const RenderPassData & d)
{
    sceneDebugRenderer.draw(d.data.viewProjMatrix);
    for (auto obj : debugSet) { obj->draw(d.data.viewProjMatrix);}

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

void VR_Renderer::run_forward_wireframe_pass(const RenderPassData & d)
{

}

void VR_Renderer::run_shadow_pass(const RenderPassData & d)
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

void VR_Renderer::run_bloom_pass(const RenderPassData & d)
{
    bloom->execute(eyeTextures[d.eye]);
    glBlitNamedFramebuffer(bloom->get_output_texture(), eyeTextures[d.eye], 0, 0, renderSizePerEye.x, renderSizePerEye.y, 0, 0, renderSizePerEye.x, renderSizePerEye.y, GL_COLOR_BUFFER_BIT, GL_LINEAR);
}

void VR_Renderer::run_reflection_pass(const RenderPassData & d)
{

}

void VR_Renderer::run_ssao_pass(const RenderPassData & d)
{

}

void VR_Renderer::run_smaa_pass(const RenderPassData & d)
{

}

void VR_Renderer::run_blackout_pass(const RenderPassData & d)
{

}

void VR_Renderer::run_post_pass(const RenderPassData & d)
{
    if (!renderPost) return;

    if (renderBloom) run_bloom_pass(d);

    if (renderReflection) run_reflection_pass(d);

    if (renderSSAO) run_ssao_pass(d);

    if (renderSMAA) run_smaa_pass(d);

    if (renderBlackout) run_blackout_pass(d);
}

void VR_Renderer::render_frame()
{
    // Renderer default state
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    // Per frame uniform buffer
    uniforms::per_scene b = {};
    b.time = timer.milliseconds().count();
    b.resolution = renderSizePerEye;
    b.invResolution = 1.f / b.resolution;
    b.activePointLights = lights.pointLights.size();

    b.directional_light.color = lights.directionalLight->color;
    b.directional_light.direction = lights.directionalLight->direction;
    b.directional_light.amount = lights.directionalLight->amount;
    for (int i = 0; i < (int)std::min(lights.pointLights.size(), size_t(4)); ++i) b.point_lights[i] = *lights.pointLights[i];

    perScene.set_buffer_data(sizeof(b), &b, GL_STREAM_DRAW);

    glBindBufferBase(GL_UNIFORM_BUFFER, uniforms::per_scene::binding, perScene);
    glBindBufferBase(GL_UNIFORM_BUFFER, uniforms::per_view::binding, perView);

    GLfloat defaultColor[] = { 0.75f, 0.75f, 0.75f, 1.0f };
    GLfloat defaultDepth = 1.f;

    // Fixme: center eye
    const ShadowData d{ shadow->get_output_texture(), lights.directionalLight->direction };
    const RenderPassData shadowPassData(0, eyes[0], d);

    if (renderShadows) run_shadow_pass(shadowPassData);

    renderTimer.start();
    for (int eyeIdx : { 0, 1 })
    {
        // Per view uniform buffer
        uniforms::per_view v = {};
        v.view = eyes[eyeIdx].pose.inverse().matrix();
        v.viewProj = mul(eyes[eyeIdx].projectionMatrix, eyes[eyeIdx].pose.inverse().matrix());
        v.eyePos = float4(eyes[eyeIdx].pose.position, 1);

        // Update render pass data. 
        eyes[eyeIdx].viewMatrix = v.view;
        eyes[eyeIdx].viewProjMatrix = v.viewProj;

        const RenderPassData renderPassData(eyeIdx, eyes[eyeIdx], d);

        if (renderShadows)
        {
            for (int c = 0; c < 4; c++)
            {
                v.cascadesPlane[c] = float4(shadow->splitPlanes[c].x, shadow->splitPlanes[c].y, 0, 0);
                v.cascadesMatrix[c] = shadow->shadowMatrices[c];
                v.cascadesNear[c] = shadow->nearPlanes[c];
                v.cascadesFar[c] = shadow->farPlanes[c];
            }
        }

        // Update the per-view info now that we've computed the cascade data
        perView.set_buffer_data(sizeof(v), &v, GL_STREAM_DRAW);
        
        // Render into 4x multisampled fbo
        glEnable(GL_MULTISAMPLE);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, multisampleFramebuffer);
        glViewport(0, 0, renderSizePerEye.x, renderSizePerEye.y);
        glClearNamedFramebufferfv(multisampleFramebuffer, GL_COLOR, 0, &defaultColor[0]);
        glClearNamedFramebufferfv(multisampleFramebuffer, GL_DEPTH, 0, &defaultDepth);

        // Execute the forward passes
        run_skybox_pass(renderPassData);

        run_forward_pass(renderPassData);
        if (renderWireframe) run_forward_wireframe_pass(renderPassData);

        glDisable(GL_MULTISAMPLE);

        // Resolve multisample into per-eye textures
        glBlitNamedFramebuffer(multisampleFramebuffer, eyeTextures[eyeIdx], 0, 0, renderSizePerEye.x, renderSizePerEye.y, 0, 0, renderSizePerEye.x, renderSizePerEye.y, GL_COLOR_BUFFER_BIT, GL_LINEAR);

        // Execute the post passes after having resolved the multisample framebuffers
        run_post_pass(renderPassData);

        gl_check_error(__FILE__, __LINE__);
    }
    renderTimer.stop();
    ImGui::Text("Render (Both Eyes) %f", renderTimer.elapsed_ms());

    bloom->gather_imgui();

    sceneDebugRenderer.clear();

    renderSet.clear();
    debugSet.clear();
}
