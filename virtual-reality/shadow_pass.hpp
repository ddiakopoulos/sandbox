#pragma once

#ifndef shadow_pass_hpp
#define shadow_pass_hpp

#include "util.hpp"
#include "linalg_util.hpp"
#include "gl-api.hpp"
#include "gl-imgui.hpp"
#include "file_io.hpp"
#include "procedural_mesh.hpp"

// http://developer.download.nvidia.com/SDK/10.5/opengl/src/cascaded_shadow_maps/doc/cascaded_shadow_maps.pdf

/*
 * To Do - 3.25.2017
 * [ ] Set shadow map resolution at runtime (default 1024^2)
 * [ ] Set number of cascades used at compile time (default 4)
 * [ ] Configurable filtering modes (ESM, PCF, PCSS + PCF)
 * [ ] Experiment with Moment Shadow Maps
 * [ ] Frustum depth-split is a good candidate for compute shader experimentation (default far-near/4)
 * [ ] Blending / overlap between cascades
 * [ ] Performance profiling
 */

using namespace avl;

struct ShadowPass
{
    GlTexture3D shadowArrayDepth;
    GlFramebuffer shadowArrayFramebuffer;

    std::vector<float4x4> viewMatrices;
    std::vector<float4x4> projMatrices;
    std::vector<float4x4> shadowMatrices;

    std::vector<float2> splitPlanes;
    std::vector<float> nearPlanes;
    std::vector<float> farPlanes;

    float resolution = 1024.0; // shadowmap resolution
    float expCascade = 150.f;  // overshadowing constant
    float splitLambda = 0.5f;  // frustum split constant

    GlMesh fsQuad;

    GlShaderHandle program;

    ShadowPass()
    {
        fsQuad = make_fullscreen_quad();

        shadowArrayDepth.setup(GL_TEXTURE_2D_ARRAY, resolution, resolution, 4, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glNamedFramebufferTextureEXT(shadowArrayFramebuffer, GL_DEPTH_ATTACHMENT, shadowArrayDepth, 0);
        shadowArrayFramebuffer.check_complete();

        gl_check_error(__FILE__, __LINE__);
    }

    void update_cascades(const float4x4 view, const float near, const float far, const float aspectRatio, const float vfov, const float3 lightDir)
    {
        nearPlanes.clear();
        farPlanes.clear();
        splitPlanes.clear();
        viewMatrices.clear();
        projMatrices.clear();
        shadowMatrices.clear();

        for (size_t C = 0; C < uniforms::NUM_CASCADES; ++C)
        {
            // Find the split planes using GPU Gem 3. Chap 10 "Practical Split Scheme".
            // http://http.developer.nvidia.com/GPUGems3/gpugems3_ch10.html

            const float splitNear = C > 0 ? mix(near + (static_cast<float>(C) / 4.0f) * (far - near),
                near * pow(far / near, static_cast<float>(C) / 4.0f), splitLambda) : near;

            const float splitFar = C < 4 - 1 ? mix(near + (static_cast<float>(C + 1) / 4.0f) * (far - near),
                near * pow(far / near, static_cast<float>(C + 1) / 4.0f), splitLambda) : far;

            const float4x4 splitProjectionMatrix = make_perspective_matrix(vfov, aspectRatio, splitNear, splitFar);

            // Extract the frustum points
            float4 splitFrustumVerts[8] = {
                { -1.f, -1.f, -1.f, 1.f }, // Near plane
                { -1.f,  1.f, -1.f, 1.f },
                { +1.f,  1.f, -1.f, 1.f },
                { +1.f, -1.f, -1.f, 1.f },
                { -1.f, -1.f,  1.f, 1.f }, // Far plane
                { -1.f,  1.f,  1.f, 1.f },
                { +1.f,  1.f,  1.f, 1.f },
                { +1.f, -1.f,  1.f, 1.f }
            };

            for (unsigned int j = 0; j < 8; ++j)
            {
                splitFrustumVerts[j] = float4(transform_coord(inverse(mul(splitProjectionMatrix, view)), splitFrustumVerts[j].xyz()), 1);
            }

            float3 frustumCentroid = float3(0, 0, 0);
            for (size_t i = 0; i < 8; ++i) frustumCentroid += splitFrustumVerts[i].xyz();
            frustumCentroid /= 8.0f;

            const float dist = std::max(splitFar - splitNear, distance(splitFrustumVerts[5], splitFrustumVerts[6]));
            const Pose cascadePose = look_at_pose_lh(frustumCentroid + (-lightDir * dist), frustumCentroid); // note the flip on the light dir here
            const float4x4 splitViewMatrix = make_view_matrix_from_pose(cascadePose);

            // Transform split vertices to the light view space
            float4 splitVerticesLightspace[8];
            for (int j = 0; j < 8; ++j)
            {
                splitVerticesLightspace[j] = float4(transform_coord(splitViewMatrix, splitFrustumVerts[j].xyz()), 1);
            }

            // Find the frustum bounding box in view space
            float4 min = splitVerticesLightspace[0];
            float4 max = splitVerticesLightspace[0];
            for (int j = 0; j < 8; ++j)
            {
                min = linalg::min(min, splitVerticesLightspace[j]);
                max = linalg::max(max, splitVerticesLightspace[j]);
            }

            // Create an orthogonal projection matrix with the corners
            const float nearOffset = 10.0f;
            const float farOffset = 32.0f;
            const float4x4 shadowProjectionMatrix = make_orthographic_matrix(min.x, max.x, min.y, max.y, -max.z - nearOffset, -min.z + farOffset);

            viewMatrices.push_back(splitViewMatrix);
            projMatrices.push_back(shadowProjectionMatrix);
            shadowMatrices.push_back(mul(shadowProjectionMatrix, splitViewMatrix));
            splitPlanes.push_back(float2(splitNear, splitFar));
            nearPlanes.push_back(-max.z - nearOffset);
            farPlanes.push_back(-min.z + farOffset);
        }

    }

    void pre_draw()
    {
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);

        glBindFramebuffer(GL_FRAMEBUFFER, shadowArrayFramebuffer);
        glViewport(0, 0, resolution, resolution);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        auto & shader = program.get();

        shader.bind();
        shader.uniform("u_cascadeNear", uniforms::NUM_CASCADES, nearPlanes);
        shader.uniform("u_cascadeFar", uniforms::NUM_CASCADES, farPlanes);
        shader.uniform("u_cascadeViewMatrixArray", uniforms::NUM_CASCADES, viewMatrices);
        shader.uniform("u_cascadeProjMatrixArray", uniforms::NUM_CASCADES, projMatrices);
    }

    void post_draw()
    {
        auto & shader = program.get();
        glCullFace(GL_BACK);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        shader.unbind();
    }

    GLuint get_output_texture() const { return shadowArrayDepth.id(); }
};

#endif // end shadow_pass_hpp
