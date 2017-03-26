#pragma once

#ifndef bloom_pass_hpp
#define bloom_pass_hpp

#include "util.hpp"
#include "linalg_util.hpp"
#include "GL_API.hpp"
#include "async_pbo.hpp"
#include "avl_imgui.hpp"
#include "file_io.hpp"
#include "procedural_mesh.hpp"

using namespace avl;

struct BloomPass
{
    union
    {
        GLuint pipelines[1];
        struct { GLuint downsample_pipeline; };
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

    GlFramebuffer brightFramebuffer, blurFramebuffer, outputFramebuffer;
    GlFramebuffer luminance[5];

    GlTexture2D brightTex, blurTex, outputTex;
    GlTexture2D luminanceTex[5];

    GlMesh fsQuad;

    float2 perEyeSize;
    float exposure = 0.5f;

    AsyncRead1 avgLuminance;

    BloomPass(float2 size) : perEyeSize(size)
    {
        fsQuad = make_fullscreen_quad();

        luminanceTex[0].setup(128, 128, GL_RGBA, GL_RGBA, GL_FLOAT, nullptr);
        luminanceTex[1].setup(64, 64, GL_RGBA, GL_RGBA, GL_FLOAT, nullptr);
        luminanceTex[2].setup(16, 16, GL_RGBA, GL_RGBA, GL_FLOAT, nullptr);
        luminanceTex[3].setup(4, 4, GL_RGBA, GL_RGBA, GL_FLOAT, nullptr);
        luminanceTex[4].setup(1, 1, GL_RGBA, GL_RGBA, GL_FLOAT, nullptr);
        brightTex.setup(perEyeSize.x / 2, perEyeSize.y / 2, GL_RGBA, GL_RGBA, GL_FLOAT, nullptr);
        blurTex.setup(perEyeSize.x / 8, perEyeSize.y / 8, GL_RGBA, GL_RGBA, GL_FLOAT, nullptr);
        outputTex.setup(perEyeSize.x, perEyeSize.y, GL_RGBA, GL_RGBA, GL_FLOAT, nullptr);

        glNamedFramebufferTexture2DEXT(luminance[0], GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, luminanceTex[0], 0);
        glNamedFramebufferTexture2DEXT(luminance[1], GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, luminanceTex[1], 0);
        glNamedFramebufferTexture2DEXT(luminance[2], GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, luminanceTex[2], 0);
        glNamedFramebufferTexture2DEXT(luminance[3], GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, luminanceTex[3], 0);
        glNamedFramebufferTexture2DEXT(luminance[4], GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, luminanceTex[4], 0);
        glNamedFramebufferTexture2DEXT(brightFramebuffer, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brightTex, 0);
        glNamedFramebufferTexture2DEXT(blurFramebuffer, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, blurTex, 0);
        glNamedFramebufferTexture2DEXT(outputFramebuffer, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outputTex, 0);

        for (auto & l : luminance) l.check_complete();
        brightFramebuffer.check_complete();
        blurFramebuffer.check_complete();
        outputFramebuffer.check_complete();

        hdr_post = GlShader(GL_VERTEX_SHADER, read_file_text("../assets/shaders/hdr/hdr_post_vert.glsl"));
        hdr_avgLumShader = GlShader(GL_FRAGMENT_SHADER, read_file_text("../assets/shaders/hdr/hdr_lumavg_frag.glsl"));

        hdr_lumShader = GlShader(read_file_text("../assets/shaders/hdr/hdr_post_vert.glsl"), read_file_text("../assets/shaders/hdr/hdr_lum_frag.glsl"));
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

        glBindFramebuffer(GL_FRAMEBUFFER, luminance[0]); // 128x128 surface area - calculate luminance 
        glViewport(0, 0, 128, 128);
        hdr_lumShader.bind();
        hdr_lumShader.texture("s_texColor", 0, sceneColorTex, GL_TEXTURE_2D);
        hdr_lumShader.uniform("u_modelViewProj", Identity4x4);
        fsQuad.draw_elements();

        {
            glBindProgramPipeline(downsample_pipeline);

            std::vector<float3> downsampleTargets = { { 0, 64, 64 },{ 1, 16, 16 },{ 2, 4, 4 },{ 3, 1, 1 } };

            auto downsample = [&](const int idx, const float2 targetSize)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, luminance[idx + 1]);
                glViewport(0, 0, targetSize.x, targetSize.y);
                hdr_avgLumShader.texture("s_texColor", 0, luminance[idx], GL_TEXTURE_2D);
                fsQuad.draw_elements();
            };

            for (auto target : downsampleTargets) downsample(target.x, float2(target.y, target.z));

            glBindProgramPipeline(0);
        }

        // Readback luminance value
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, luminance[4]);
        auto lumValue = avgLuminance.download();
        glBindTexture(GL_TEXTURE_2D, 0);

        float4 tonemap = { middleGrey, whitePoint * whitePoint, threshold, 0.0f };

        /*
        const float lumTarget = 0.4f;
        const float exposureTarget = lumValue.x += 0.1 * (lumTarget - lumValue.x);
        float exposureCtrl = 0.86f;
        exposureCtrl = exposureCtrl * 0.1 + exposureTarget * 0.9;
        const float exposure = std::exp(exposureCtrl*exposureCtrl) - 1.0f;
        ImGui::Text("Exposure %f", exposure);
        */

        ImGui::SliderFloat("MiddleGrey", &middleGrey, 0.1f, 1.0f);
        ImGui::SliderFloat("WhitePoint", &whitePoint, 0.1f, 2.0f);
        ImGui::SliderFloat("Threshold", &threshold, 0.1f, 2.0f);
        ImGui::SliderFloat("Exposure", &exposure, 0.1f, 2.0f);
        //ImGui::Text("Luminance %f", lumValue.x);

        glBindFramebuffer(GL_FRAMEBUFFER, brightFramebuffer);
        glViewport(0, 0, perEyeSize.x / 2, perEyeSize.y / 2);
        hdr_brightShader.bind();
        hdr_brightShader.texture("s_texColor", 0, sceneColorTex, GL_TEXTURE_2D);
        hdr_brightShader.uniform("u_exposure", exposure);
        hdr_brightShader.uniform("u_tonemap", tonemap);
        hdr_brightShader.uniform("u_modelViewProj", Identity4x4);
        fsQuad.draw_elements();

        glBindFramebuffer(GL_FRAMEBUFFER, blurFramebuffer);
        glViewport(0, 0, perEyeSize.x / 8, perEyeSize.y / 8);
        hdr_blurShader.bind();
        hdr_blurShader.texture("s_texColor", 0, brightTex, GL_TEXTURE_2D);
        hdr_blurShader.uniform("u_viewTexel", float2(1.f / (perEyeSize.x / 8.f), 1.f / (perEyeSize.y / 8.f)));
        hdr_blurShader.uniform("u_modelViewProj", Identity4x4);
        fsQuad.draw_elements();

        glBindFramebuffer(GL_FRAMEBUFFER, outputFramebuffer);
        glViewport(0, 0, perEyeSize.x, perEyeSize.y);
        hdr_tonemapShader.bind();
        hdr_tonemapShader.texture("s_texColor", 0, sceneColorTex, GL_TEXTURE_2D);
        hdr_tonemapShader.texture("s_texBright", 1, brightTex, GL_TEXTURE_2D);
        hdr_tonemapShader.uniform("u_exposure", exposure);
        hdr_tonemapShader.uniform("u_tonemap", tonemap);
        fsQuad.draw_elements();

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_FRAMEBUFFER_SRGB);
    }

    GLuint get_output_texture() const { return outputFramebuffer.id(); }

    GLuint get_luminance_texture() const { return luminanceTex[0].id(); }

    GLuint get_bright_tex() const { return brightTex.id(); }

    GLuint get_blur_tex() const { return blurTex.id(); }
};

#endif // end bloom_pass_hpp
