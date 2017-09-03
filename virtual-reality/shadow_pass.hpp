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
// https://www.gamedev.net/forums/topic/497259-stable-cascaded-shadow-maps/
// https://github.com/jklarowicz/dx11_samples/blob/master/VarianceShadows11/VarianceShadowsManager.cpp
// https://github.com/TheRealMJP/Shadows/blob/master/Shadows/MeshRenderer.cpp
// http://the-witness.net/news/2010/03/graphics-tech-shadow-maps-part-1/

/*
 * To Do - 3.25.2017
 * [ ] Set shadow map resolution at runtime (default 1024^2)
 * [X] Set number of cascades used at compile time (default 4)
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

    float resolution = 1024.f; // shadowmap resolution
    float splitLambda = 0.5f;  // frustum split constant
    float nearOffset = 12.0f;
    float farOffset = 24.0f;
    bool stabilize = true;

    // float expCascade = 150.f;  // overshadowing constant

    GlMesh fsQuad;

    GlShaderHandle program;

    ShadowPass()
    {
        fsQuad = make_fullscreen_quad();

        shadowArrayDepth.setup(GL_TEXTURE_2D_ARRAY, resolution, resolution, uniforms::NUM_CASCADES, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
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

            const float splitIdx = uniforms::NUM_CASCADES;

            const float splitNear = C > 0 ? mix(near + (static_cast<float>(C) / splitIdx) * (far - near),
                near * pow(far / near, static_cast<float>(C) / splitIdx), splitLambda) : near;

            const float splitFar = C < splitIdx - 1 ? mix(near + (static_cast<float>(C + 1) / splitIdx) * (far - near),
                near * pow(far / near, static_cast<float>(C + 1) / splitIdx), splitLambda) : far;

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
            Pose cascadePose = look_at_pose_lh(frustumCentroid + (-lightDir * dist), frustumCentroid); // note the flip on the light dir here
            float4x4 splitViewMatrix = make_view_matrix_from_pose(cascadePose);

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

            float3 minExtents;
            float3 maxExtents;

            if (stabilize)
            {
                // Calculate the radius of a bounding sphere surrounding the frustum corners in worldspace
                float sphereRadius = 0.0f;
                for (int i = 0; i < 8; ++i)
                {
                    float dist = length(splitFrustumVerts[i].xyz() - frustumCentroid);
                    sphereRadius = std::max(sphereRadius, dist);
                }

                sphereRadius = std::ceil(sphereRadius * 16.0f) / 16.0f;

                maxExtents = float3(sphereRadius, sphereRadius, sphereRadius);
                minExtents = -maxExtents;
            }

            cascadePose = look_at_pose_lh(frustumCentroid + (-lightDir) * minExtents.z, frustumCentroid); // note the flip on the light dir here. LH?
            splitViewMatrix = make_view_matrix_from_pose(cascadePose); 

            float3 cascadeExtents = maxExtents - minExtents;

            minExtents = transform_coord(splitViewMatrix, minExtents);
            maxExtents = transform_coord(splitViewMatrix, maxExtents);
            cascadeExtents = transform_coord(splitViewMatrix, cascadeExtents);

            std::cout << "Min Real:    " << make_orthographic_matrix(min.x, max.x, min.y, max.y, -max.z - nearOffset, -min.z + farOffset) << std::endl;
            std::cout << "Min Extents: " << make_orthographic_matrix(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0, cascadeExtents.z) << std::endl;
            float4x4 shadowProjectionMatrix;
            
            if (stabilize)
                shadowProjectionMatrix = make_orthographic_matrix(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0, cascadeExtents.z);
            else
                shadowProjectionMatrix = make_orthographic_matrix(min.x, max.x, min.y, max.y, -max.z - nearOffset, -min.z + farOffset);

            // Because of the tight-fit, the shimmering shadow edges will still exist when the camera rotates
            if (stabilize)
            {
                // Create a rounding matrix by projecting the light-space origin and determining the fractional offset in texel space
                float3 shadowOrigin = transform_coord(mul(shadowProjectionMatrix, splitViewMatrix), float3(0, 0, 0));
                shadowOrigin *= (resolution * 0.5f);

                float4 roundedOrigin = round(float4(shadowOrigin, 1));
                float4 roundOffset = roundedOrigin - float4(shadowOrigin, 1);
                roundOffset *= 2.0f / resolution;
                roundOffset.z = 0;
                roundOffset.w = 0;

                shadowProjectionMatrix[3] += roundOffset;
            }

            viewMatrices.push_back(splitViewMatrix);
            projMatrices.push_back(shadowProjectionMatrix);
            shadowMatrices.push_back(mul(shadowProjectionMatrix, splitViewMatrix));
            splitPlanes.push_back(float2(splitNear, splitFar));
            nearPlanes.push_back(-maxExtents.z);
            farPlanes.push_back(-minExtents.z);
        }

    }

    void gather_imgui(const bool enabled)
    {
        if (!enabled) return;
        ImGui::Checkbox("Stabilize", &stabilize);
        ImGui::SliderFloat("Near Offset", &nearOffset, 0.0f, 128.0f);
        ImGui::SliderFloat("Far Offset", &farOffset, 0.0f, 128.0f);
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
