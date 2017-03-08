#include "vr_renderer.hpp"
#include "material.hpp"

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

    // Generate textures and framebuffers for the left and right eye images
    for (int eye : { (int) Eye::LeftEye, (int) Eye::RightEye })
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
}

VR_Renderer::~VR_Renderer()
{

}

void VR_Renderer::set_eye_data(const EyeData left, const EyeData right)
{
    eyes[0] = left;
    eyes[1] = right;
}

void VR_Renderer::run_skybox_pass()
{

}

void VR_Renderer::run_forward_pass(const uniforms::per_view & uniforms)
{
    for (auto obj : debugSet)
    {
        obj->draw(uniforms.viewProj);
    }

    for (auto obj : renderSet)
    {
        const float4x4 modelMatrix = mul(obj->get_pose().matrix(), make_scaling_matrix(obj->get_scale()));
        Material * mat = obj->get_material();

        if (mat)
        {
            mat->update_uniforms();
            mat->use(modelMatrix);
            obj->draw();
        }
        else
        {
            // Do I want to use exceptions in the renderer?
            throw std::runtime_error("cannot draw object without bound material");
        }
    }
}

void VR_Renderer::run_forward_wireframe_pass()
{

}

void VR_Renderer::run_shadow_pass()
{

}

void VR_Renderer::run_bloom_pass()
{

}

void VR_Renderer::run_reflection_pass()
{

}

void VR_Renderer::run_ssao_pass()
{

}

void VR_Renderer::run_smaa_pass()
{

}

void VR_Renderer::run_blackout_pass()
{

}

void VR_Renderer::run_post_pass()
{
    if (!renderPost) return;

    if (renderBloom) run_bloom_pass();

    if (renderReflection) run_reflection_pass();

    if (renderSSAO) run_ssao_pass();

    if (renderSMAA) run_smaa_pass();

    if (renderBlackout) run_blackout_pass();
}

void VR_Renderer::render_frame()
{
    // Renderer default state
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    // Per frame uniform buffer
    uniforms::per_scene b = {};
    b.time = 0.0f;
    perScene.set_buffer_data(sizeof(b), &b, GL_STREAM_DRAW);

    glBindBufferBase(GL_UNIFORM_BUFFER, uniforms::per_scene::binding, perScene);
    glBindBufferBase(GL_UNIFORM_BUFFER, uniforms::per_view::binding, perView);

    GLfloat defaultColor[] = { 0.75f, 0.75f, 0.75f, 1.0f };
    GLfloat defaultDepth = 1.f;

    for (int eye : { (int)Eye::LeftEye, (int) Eye::RightEye })
    {
        // Per view uniform buffer
        uniforms::per_view v = {};
        v.view = eyes[eye].pose.inverse().matrix();
        v.viewProj = mul(eyes[eye].projectionMatrix, eyes[eye].pose.inverse().matrix());
        v.eyePos = eyes[eye].pose.position;
        perView.set_buffer_data(sizeof(v), &v, GL_STREAM_DRAW);

        glViewport(0, 0, renderSizePerEye.x, renderSizePerEye.y);

        Frustum f(v.viewProj);
        
        for (auto p : f.get_corners())
        {
            std::cout << "Pt: " << p << std::endl;
        }
        std::cout << "-----------------" << std::endl;

        // Render into 4x multisampled fbo
        glEnable(GL_MULTISAMPLE);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, multisampleFramebuffer);

        glClearNamedFramebufferfv(multisampleFramebuffer, GL_COLOR, 0, &defaultColor[0]);
        glClearNamedFramebufferfv(multisampleFramebuffer, GL_DEPTH, 0, &defaultDepth);

        // Execute the forward passes
        run_skybox_pass();

        run_forward_pass(v);

        if (renderWireframe) run_forward_wireframe_pass();
        if (renderShadows) run_shadow_pass();

        glDisable(GL_MULTISAMPLE);

        // Resolve multisample into per-eye textures
        glBlitNamedFramebuffer(multisampleFramebuffer, eyeTextures[eye], 0, 0, renderSizePerEye.x, renderSizePerEye.y, 0, 0, renderSizePerEye.x, renderSizePerEye.y, GL_COLOR_BUFFER_BIT, GL_LINEAR);

        // Execute the post passes after having resolved the multisample framebuffers
        run_post_pass();
    }

    renderSet.clear();
    debugSet.clear();
}
