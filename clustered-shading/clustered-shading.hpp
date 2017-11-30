#pragma once

/*
 * This file implements a minimal viable implementation of clustered forward shading, 
 * currently only supporting point light sources. Clustered forward shading is an 
 * improvement over Forward Plus and Tiled Deferred shading, but not as state-of-the-art as
 * Yuri O'Donnell's Tiled Light Trees (2017). A major benefit of these 'modern' forward
 * approaches is that transparency and MSAA "just works" and supports a large number
 * of dynamic lights, an ideal fit for the requirements of rendering for virtual reality.
 * 
 * Clustered shading is not an ideal term for this technique, since it extends Forward 
 * Plus on the Z axis and does not perform clustering in the statistical meaning of 
 * the word. A traditional forward plus implementation requires a z-prepass and 
 * computes lists of lights affected a 2D lighting grid, often leveraging a compute 
 * shader to perform the necessary per-tile culling.
 * 
 * This implementation is based on "Practical Clustered Shading (2012)" proposed
 * by Emil Persson, aka Humus. Light clustering is performed on the CPU and does 
 * not require a z-prepass. The viewing frustum is divided into a 3D grid of lights, 
 * where light-to-cluster assignment is performed in clipspace, sometimes called 
 * frustum voxels (froxels). Since VR experiences are often fragment-bound instead of 
 * vertex-bound, the lack of an extra z-prepass and compute shader theoretically enables
 * more GPU headroom for shading calculations. 
 * 
 * The crux of clustered shading is to compute a tight froxel fit around light sources 
 * and pack this information in a way such that we can efficiently leverage 
 * the dynamic branching capabilities of newer GPUs. Clusters are available to the 
 * GPU as a 3D texture consisting of an offset to lighting indices affecting the 
 * cluster, and the number of lights. The lighting index buffer is a tightly packed 
 * array which stores an index value to the actual array of scene lights, sorted
 * by 3D cluster coordinate. 
 *
 * Useful References on Clustered Shading:
 * [1] http://www.humus.name/Articles/PracticalClusteredShading.pdf
 * [2] http://www.adriancourreges.com/blog/2016/09/09/doom-2016-graphics-study/
 * [3] https://developer.oculus.com/blog/introducing-the-oculus-unreal-renderer/
 * [4] http://diglib.eg.org/handle/10.2312/EGGH.HPG12.087-096
 * [5] https://www.3dgep.com/forward-plus/
 */

#ifndef clustered_shading_hpp
#define clustered_shading_hpp

#include "linalg_util.hpp"
#include "gl-api.hpp"
#include "gl-mesh.hpp"
#include "geometric.hpp"
#include "geometry.hpp"

using namespace avl;

void draw_debug_frustum(GlShader * shader, const Frustum & f, const float4x4 & renderViewProjMatrix, const float4 & color)
{
    auto generated_frustum_corners = make_frustum_corners(f);

    float3 ftl = generated_frustum_corners[0];
    float3 fbr = generated_frustum_corners[1];
    float3 fbl = generated_frustum_corners[2];
    float3 ftr = generated_frustum_corners[3];
    float3 ntl = generated_frustum_corners[4];
    float3 nbr = generated_frustum_corners[5];
    float3 nbl = generated_frustum_corners[6];
    float3 ntr = generated_frustum_corners[7];

    std::vector<float3> frustum_coords = {
        ntl, ntr, ntr, nbr, nbr, nbl, nbl, ntl, // near quad
        ntl, ftl, ntr, ftr, nbr, fbr, nbl, fbl, // between
        ftl, ftr, ftr, fbr, fbr, fbl, fbl, ftl, // far quad
    };

    Geometry g;
    for (auto & v : frustum_coords) g.vertices.push_back(v);
    GlMesh mesh = make_mesh_from_geometry(g);
    mesh.set_non_indexed(GL_LINES);

    // Draw debug visualization 
    shader->bind();
    shader->uniform("u_mvp", mul(renderViewProjMatrix, Identity4x4));
    shader->uniform("u_color", color);
    mesh.draw_elements();
    shader->unbind();
}

namespace uniforms
{
    static const size_t MAX_POINT_LIGHTS = 1024;

    struct point_light
    {
        ALIGNED(16) float4 positionRadius;
        ALIGNED(16) float4 colorIntensity;
    };

    struct clustered_lighting_buffer
    {
        static const int binding = 7;
        point_light lights[MAX_POINT_LIGHTS];
    };
};

// Based on sample code provided in "2D Polyhedral Bounds of a Clipped, Perspective-Projected 3D Sphere"
// http://jcgt.org/published/0002/02/05/paper.pdf
inline Bounds3D sphere_for_axis(const float3 & axis, const float3 & sphereCenter, const float sphereRadius, const float zNearClipCamera)
{
    const bool sphereClipByZNear = !((sphereCenter.z + sphereRadius) < zNearClipCamera);
    const float2 projectedCenter = float2(dot(axis, sphereCenter), sphereCenter.z);
    const float tSquared = dot(projectedCenter, projectedCenter) - (sphereRadius * sphereRadius);

    float t, cLength, cosTheta, sintheta;

    const bool outsideSphere = (tSquared > 0);

    if (outsideSphere)
    {
        // cosTheta, sinTheta of angle between the projected center (in a-z space) and a tangent
        t = sqrt(tSquared);
        cLength = length(projectedCenter);
        cosTheta = t / cLength;
        sintheta = sphereRadius / cLength;
    }

    // Square root of the discriminant; NaN (and unused) if the camera is in the sphere
    float sqrtPart = 0.f;
    if (sphereClipByZNear)
    {
        sqrtPart = std::sqrt((sphereRadius * sphereRadius) - ((zNearClipCamera - projectedCenter.y) * (zNearClipCamera - projectedCenter.y)));
    }

    float2 bounds[2]; // in the a-z reference frame

    sqrtPart *= -1.f;
    for (int i = 0; i < 2; ++i)
    {
        if (tSquared >  0)
        {
            const float2x2 rotator = { { cosTheta, -sintheta },{ sintheta, cosTheta } };
            const float2 rotated = normalize(mul(rotator, projectedCenter));
            bounds[i] = cosTheta * mul(rotator, projectedCenter);
        }

        if (sphereClipByZNear && (!outsideSphere || bounds[i].y > zNearClipCamera))
        {
            bounds[i].x = projectedCenter.x + sqrtPart;
            bounds[i].y = zNearClipCamera;
        }
        sintheta *= -1.f;
        sqrtPart *= -1.f;
    }

    Bounds3D boundsViewSpace;
    boundsViewSpace._min = bounds[0].x * axis;
    boundsViewSpace._min.z = bounds[0].y;
    boundsViewSpace._max = bounds[1].x * axis;
    boundsViewSpace._max.z = bounds[1].y;

    return boundsViewSpace;
}

struct ClusteredShading
{
    static const int32_t NumClustersX = 16; // Tiles in X
    static const int32_t NumClustersY = 16; // Tiles in Y
    static const int32_t NumClustersZ = 16; // Slices in Z

    float nearClip, farClip;
    float vFov;
    float aspect;

    GlBuffer lightingBuffer;
    GlBuffer lightIndexBuffer;
    GlTexture2D lightIndexTexture;
    GlTexture3D clusterTexture;

    enum class LightType
    {
        Spherical,
        Spot,
        Area
    };

    // This is stored in a 3D texture (clusterTexture => GL_RG32UI)
    struct ClusterPointer
    {
        uint32_t offset = 0;
        uint32_t lightCount = 0;
    };

    std::vector<ClusterPointer> clusterTable;
    std::vector<std::pair<uint16_t, uint16_t>> lightIndices;
    uint32_t numLightIndices = 0;

    static const size_t maxLights = std::numeric_limits<uint16_t>::max() * 8;

    ClusteredShading(float vFov, float aspect, float nearClip, float farClip) : vFov(vFov), aspect(aspect), nearClip(nearClip), farClip(farClip)
    {
        clusterTable.resize(NumClustersX * NumClustersY * NumClustersZ);
        lightIndices.resize(maxLights);

        // Setup 3D cluster texture
        clusterTexture.setup(GL_TEXTURE_3D, NumClustersX, NumClustersY, NumClustersZ, GL_RG32UI, GL_RG_INTEGER, GL_UNSIGNED_INT, nullptr);
        glTextureParameteriEXT(clusterTexture, GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTextureParameteriEXT(clusterTexture, GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        // Setup the index light buffer
        glNamedBufferData(lightIndexBuffer, maxLights * sizeof(uint16_t), nullptr, GL_DYNAMIC_DRAW); // DSA glBufferData

        // Setup the light index texture
        GLuint lib;
        glCreateTextures(GL_TEXTURE_BUFFER, 1, &lib);
        lightIndexTexture = GlTexture2D(lib);
        glTextureBuffer(lightIndexTexture, GL_R16UI, lightIndexBuffer); // DSA for glTexBuffer

        gl_check_error(__FILE__, __LINE__);
    }

    void cull_lights(const float4x4 & viewMatrix, const float4x4 & projectionMatrix, const std::vector<uniforms::point_light> & lights)
    {
        manual_timer t;

        t.start();

        // Reset state
        lightIndices.clear();
        for (auto & c : clusterTable) c = {};
        numLightIndices = 0;

        uint32_t visibleLightCount = 0;
        Frustum cameraFrustum(mul(projectionMatrix, viewMatrix));

        float nearFarDistanceRCP = 1.0f / (farClip - nearClip);

        for (int lightIndex = 0; lightIndex < lights.size(); ++lightIndex)
        {
            const uniforms::point_light & l = lights[lightIndex];

            // Conservative light culling based on worldspace camera frustum
            if (!cameraFrustum.intersects(l.positionRadius.xyz(), l.positionRadius.w))
            {
                continue;
            }

            visibleLightCount++;

            // Convert sphere to froxel bounds 
            const float3 lightCenterVS = transform_coord(viewMatrix, l.positionRadius.xyz());
            const float nearClipVS = -nearClip;

            const Bounds3D leftRightViewSpace = sphere_for_axis(float3(1, 0, 0), lightCenterVS, l.positionRadius.w, nearClipVS);
            const Bounds3D bottomTopViewSpace = sphere_for_axis(float3(0, 1, 0), lightCenterVS, l.positionRadius.w, nearClipVS);
            const float linearDepthMin = (-lightCenterVS.z - l.positionRadius.w) * nearFarDistanceRCP;
            const float linearDepthMax = (-lightCenterVS.z + l.positionRadius.w) * nearFarDistanceRCP;

            Bounds3D sphereClipSpace;
            sphereClipSpace._min = float3(transform_coord(projectionMatrix, leftRightViewSpace.min()).x, transform_coord(projectionMatrix, bottomTopViewSpace.min()).y, linearDepthMin);
            sphereClipSpace._max = float3(transform_coord(projectionMatrix, leftRightViewSpace.max()).x, transform_coord(projectionMatrix, bottomTopViewSpace.max()).y, linearDepthMax);
            sphereClipSpace._min = clamp(sphereClipSpace._min, float3(-1.f), float3(1.f)); // projected clip space can go out of the unit cube, so clamp 
            sphereClipSpace._max = clamp(sphereClipSpace._max, float3(-1.f), float3(1.f));

            // Get the clip-space min/max extents of the sphere clamped to voxel boundaries. This will give us AABB cluster indices => clusterID.
            const float z0 = (int)std::max(0, std::min((int)(linearDepthMin * (float)NumClustersZ), NumClustersZ - 1));
            const float z1 = (int)std::max(0, std::min((int)(linearDepthMax * (float)NumClustersZ), NumClustersZ - 1));
            const float y0 = (int)std::min((int)((sphereClipSpace._min.y * 0.5f + 0.5f) * (float)NumClustersY), NumClustersY - 1);
            const float y1 = (int)std::min((int)((sphereClipSpace._max.y * 0.5f + 0.5f) * (float)NumClustersY), NumClustersY - 1);
            const float x0 = (int)std::min((int)((sphereClipSpace._min.x * 0.5f + 0.5f) * (float)NumClustersX), NumClustersX - 1);
            const float x1 = (int)std::min((int)((sphereClipSpace._max.x * 0.5f + 0.5f) * (float)NumClustersX), NumClustersX - 1);

            const Bounds3D voxelsOverlappingSphere({ x0, y0, z0 }, { x1, y1, z1 });

            for (int z = voxelsOverlappingSphere._min.z; z <= voxelsOverlappingSphere._max.z; z++)
            {
                for (int y = voxelsOverlappingSphere._min.y; y <= voxelsOverlappingSphere._max.y; y++)
                {
                    for (int x = voxelsOverlappingSphere._min.x; x <= voxelsOverlappingSphere._max.x; x++)
                    {
                        const uint16_t clusterId = z * (NumClustersX * NumClustersY) + y * NumClustersX + x;
                        if (clusterId >= clusterTable.size()) continue;
                        clusterTable[clusterId].lightCount += 1;

                        if (numLightIndices > lightIndices.size()) continue; // actually return, can't handle any more lights

                        // We need the cluster ID and to know what light IDs are assigned to it. We'll do this
                        // by sorting on the cluster id, and then by the light index. The actual sort is performed later.
                        lightIndices.push_back({clusterId, lightIndex});
                        numLightIndices += 1;
                    }
                }
            }
        }

        t.stop();

        ImGui::Text("Visible Lights %i", visibleLightCount);
        ImGui::Text("Cluster Generation CPU %f ms", t.get());
    }

    void upload(std::vector<uniforms::point_light> & lights)
    {
        manual_timer t;
        t.start();

        // Sort the list
        std::sort(lightIndices.begin(), lightIndices.end());

        // Indices are tightly packed
        std::vector<uint16_t> packedLightIndices;
        uint16_t lastClusterID = -1;
        for (int i = 0; i < numLightIndices; ++i)
        {
            uint16_t clusterId = lightIndices[i].first;

            // New cluster. One cluster can hold many lights, but we only need to store the offset to
            // the first one in the list. 
            if (clusterId != lastClusterID)
            {
                auto currentLightIndex = packedLightIndices.size();
                clusterTable[clusterId].offset = currentLightIndex;
            }

            packedLightIndices.push_back(lightIndices[i].second);
            lastClusterID = clusterId;
        }
        
        // Update clustered lighting UBO
        glBindBufferBase(GL_UNIFORM_BUFFER, uniforms::clustered_lighting_buffer::binding, lightingBuffer);
        uniforms::clustered_lighting_buffer lighting = {};
        for (int l = 0; l < lights.size(); l++) lighting.lights[l] = lights[l];
        lightingBuffer.set_buffer_data(sizeof(lighting), &lighting, GL_STREAM_DRAW);

        // Update Index Data
        glBindBuffer(GL_TEXTURE_BUFFER, lightIndexBuffer);
        lightIndexBuffer.set_buffer_data(sizeof(uint16_t) * packedLightIndices.size(), packedLightIndices.data(), GL_STREAM_DRAW); // fixme to use subData

        // Update cluster grid
        glTextureSubImage3D(clusterTexture, 0, 0, 0, 0, NumClustersX, NumClustersY, NumClustersZ, GL_RG_INTEGER, GL_UNSIGNED_INT, (void *)clusterTable.data());

        t.stop();

        ImGui::Text("Uploaded %i lights indices to the lighting buffer", numLightIndices);
        ImGui::Text("Uploaded %i bytes to the index buffer", sizeof(uint16_t) * lightIndices.size());
        ImGui::Text("Sorted List Generation CPU %f ms", t.get());

        gl_check_error(__FILE__, __LINE__);
    }
};

inline std::vector<Frustum> build_debug_froxel_array(const ClusteredShading & clusterer, const float4x4 & viewMatrix)
{
    std::vector<Frustum> froxels;

    const float stepZ = (clusterer.farClip - clusterer.nearClip) / clusterer.NumClustersZ;

    for (int z = 0; z < clusterer.NumClustersZ; z++)
    {
        const float near = clusterer.nearClip + (stepZ * z);
        const float far = near + stepZ;

        const float top = near * std::tan(clusterer.vFov * 0.5f); // normalized height
        const float right = top * clusterer.aspect; // normalized width
        const float left = -right;
        const float bottom = -top;

        const float stepX = (right * 2.0f) / clusterer.NumClustersX;
        const float stepY = (top   * 2.0f) / clusterer.NumClustersY;

        float L, R, B, T;

        for (int y = 0; y < clusterer.NumClustersY; y++)
        {
            for (int x = 0; x < clusterer.NumClustersX; x++)
            {
                L = left + (stepX * x);
                R = L + stepX;
                B = bottom + (stepY * y);
                T = B + stepY;

                const float4x4 projectionMatrix = make_projection_matrix(L, R, B, T, near, far);
                const Frustum froxel(mul(projectionMatrix, viewMatrix));
                froxels.push_back(froxel);
            }
        }
    }

    return froxels;
}

#endif // end clustered_shading_hpp