#include "vr_renderer.hpp"
#include "material.hpp"
#include "avl_imgui.hpp"

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

}

// Do I want to use exceptions in the renderer?
void VR_Renderer::run_forward_pass(const RenderPassData & d)
{
    sceneDebugRenderer.draw(d.perView.viewProj);
    for (auto obj : debugSet) { obj->draw(d.perView.viewProj);}

    std::sort(renderSet.begin(), renderSet.end(), [](Renderable * lhs, Renderable * rhs)
    {
        return (lhs->get_material()->id() < rhs->get_material()->id());
    });

    for (auto obj : renderSet)
    {
        if (Material * mat = obj->get_material())
        {
            mat->update_uniforms(&d);
            mat->use(mul(obj->get_pose().matrix(), make_scaling_matrix(obj->get_scale())), d.perView.view);
            obj->draw();
        }
        else throw std::runtime_error("cannot draw object without bound material");
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
    shadow->update_cascades(make_view_matrix_from_pose(d.data.pose), d.data.nearClip, d.data.farClip, d.data.aspectRatio, d.data.vfov, d.perScene.directional_light.direction);

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
    const RenderPassData shadowData(0, eyes[0], b, {}, 0);

    if (renderShadows) run_shadow_pass(shadowData);

    renderTimer.start();
    for (int eyeIdx : { 0, 1 })
    {
        // Per view uniform buffer
        uniforms::per_view v = {};
        v.view = eyes[eyeIdx].pose.inverse().matrix();
        v.viewProj = mul(eyes[eyeIdx].projectionMatrix, eyes[eyeIdx].pose.inverse().matrix());
        v.eyePos = float4(eyes[eyeIdx].pose.position, 1);

        const RenderPassData renderPassData(eyeIdx, eyes[eyeIdx], b, v, shadow->shadowArrayDepth);

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

    sceneDebugRenderer.clear();

    renderSet.clear();
    debugSet.clear();
}
