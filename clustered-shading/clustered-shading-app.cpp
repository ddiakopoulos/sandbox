#include "index.hpp"
#include "imgui/imgui_internal.h"
#include "clustered-shading-app.hpp"

using namespace avl;

constexpr const char default_color_vert[] = R"(#version 330
    layout(location = 0) in vec3 vertex;
    uniform mat4 u_mvp;
    void main()
    {
        gl_Position = u_mvp * vec4(vertex.xyz, 1);
    }
)";

constexpr const char default_color_frag[] = R"(#version 330
    out vec4 f_color;
    uniform vec4 u_color;
    void main()
    {
        f_color = vec4(u_color);
    }
)";

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

// http://www.humus.name/Articles/PracticalClusteredShading.pdf
struct ClusteredLighting
{
    static const int32_t NumClustersX = 8; // Tiles in X
    static const int32_t NumClustersY = 8; // Tiles in Y
    static const int32_t NumClustersZ = 8; // Slices in Z

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

    // This is stored in a 3D texture (clusterTexture => R32G32_UINT)
    struct ClusterPointer
    {
        uint32_t offset = 0; // offset into 
        uint32_t lightCount = 0;
    };

    std::vector<ClusterPointer> clusterTable;
    std::vector<uint16_t> lightIndices;         // light ids only
    std::vector<uint16_t> lightClusterIDs;      // clusterId
    uint32_t numLightIndices = 0;

    static const uint16_t maxLights = std::numeric_limits<uint16_t>::max();

    ClusteredLighting(float vFov, float aspect, float nearClip, float farClip) : vFov(vFov), aspect(aspect), nearClip(nearClip), farClip(farClip)
    {
        clusterTable.resize(NumClustersX * NumClustersY * NumClustersZ);
        lightIndices.resize(maxLights);
        lightClusterIDs.resize(maxLights);
        
        // Setup 3D cluster texture
        clusterTexture.setup(GL_TEXTURE_3D, NumClustersX, NumClustersY, NumClustersZ, GL_RG32UI, GL_RG_INTEGER, GL_UNSIGNED_INT, nullptr);
        glTextureParameteriEXT(clusterTexture, GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTextureParameteriEXT(clusterTexture, GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        gl_check_error(__FILE__, __LINE__);

        std::cout << "Light Index Buffer: " << lightIndexBuffer.id() << std::endl;

        glNamedBufferData(lightIndexBuffer, maxLights * sizeof(uint16_t), nullptr, GL_DYNAMIC_DRAW); // DSA glBufferData

        gl_check_error(__FILE__, __LINE__);

        std::cout << "Light Tex Buffer: " << lightIndexTexture.id() << std::endl;

        GLuint lib;
        glCreateTextures(GL_TEXTURE_BUFFER, 1, &lib);
        lightIndexTexture = GlTexture2D(lib);
        glTextureBuffer(lightIndexTexture, GL_R16UI, lightIndexBuffer); // DSA for glTexBuffer

        gl_check_error(__FILE__, __LINE__);
    }

    void cull_lights(const float4x4 & viewMatrix, const float4x4 & projectionMatrix, const std::vector<uniforms::point_light> & lights)
    {
        // Reset state
        for (auto & i : lightClusterIDs) i = 0;
        for (auto & i : lightIndices) i = 0;
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

            auto viewDepthToFroxelDepth = [&](float viewspaceDepth)
            {
                float vZ = (viewspaceDepth - nearClip) / (farClip - nearClip);
                //float fZ = std::pow(vZ, 1 / 2.0f); // fixme, distribution factor 
                return clamp(vZ, 0.f, 1.f);
            };

            // Convert sphere to froxel bounds 
            const float3 lightCenterVS = transform_coord(viewMatrix, l.positionRadius.xyz());
            const float nearClipVS = -nearClip;

            const float linearDepthMin = (-lightCenterVS.z - l.positionRadius.w - nearClip) * nearFarDistanceRCP;
            const float linearDepthMax = (-lightCenterVS.z + l.positionRadius.w - nearClip) * nearFarDistanceRCP;

            const Bounds3D leftRightViewSpace = sphere_for_axis(float3(1, 0, 0), lightCenterVS, l.positionRadius.w, -nearClipVS);
            const Bounds3D bottomTopViewSpace = sphere_for_axis(float3(0, 1, 0), lightCenterVS, l.positionRadius.w, -nearClipVS);

            Bounds3D sphereClipSpace;
            sphereClipSpace._min = float3(transform_coord(projectionMatrix, leftRightViewSpace.min()).x, transform_coord(projectionMatrix, bottomTopViewSpace.min()).y, viewDepthToFroxelDepth(linearDepthMin));
            sphereClipSpace._max = float3(transform_coord(projectionMatrix, leftRightViewSpace.max()).x, transform_coord(projectionMatrix, bottomTopViewSpace.max()).y, viewDepthToFroxelDepth(linearDepthMax));

            // Get the clip-space min, max extents of the sphere clamped to voxel boundaries. 
            const float z0 = (int)std::max(0, std::min((int)(linearDepthMin * (float)NumClustersZ), NumClustersZ - 1));
            const float z1 = (int)std::max(0, std::min((int)(linearDepthMax * (float)NumClustersZ), NumClustersZ - 1));
            const float y0 = (int)std::min((int)((sphereClipSpace._min.y * 0.5f + 0.5f) * (float)NumClustersY), NumClustersY - 1);
            const float y1 = (int)std::min((int)((sphereClipSpace._max.y * 0.5f + 0.5f) * (float)NumClustersY), NumClustersY - 1);
            const float x0 = (int)std::min((int)((sphereClipSpace._min.x * 0.5f + 0.5f) * (float)NumClustersX), NumClustersX - 1);
            const float x1 = (int)std::min((int)((sphereClipSpace._max.x * 0.5f + 0.5f) * (float)NumClustersX), NumClustersX - 1);

            Bounds3D voxelsOverlappingSphere({ x0, y0, z0 }, { x1, y1, z1 });

            for (int z = voxelsOverlappingSphere._min.z; z <= voxelsOverlappingSphere._max.z; z++)
            {
                for (int y = voxelsOverlappingSphere._min.y; y <= voxelsOverlappingSphere._max.y; y++)
                {
                    for (int x = voxelsOverlappingSphere._min.x; x <= voxelsOverlappingSphere._max.x; x++)
                    {
                        const uint16_t clusterId = z * (NumClustersX * NumClustersY) + y * NumClustersX + x;
                        if (clusterId >= clusterTable.size()) continue; // todo - runtime assert max clusters. also there's an issue with spheres close to the nearclip. 
                        clusterTable[clusterId].lightCount += 1;
                        lightIndices[numLightIndices] = lightIndex;
                        lightClusterIDs[numLightIndices] = clusterId;
                        numLightIndices += 1;
                    }
                }
            }
        }

        ImGui::Text("Visible Lights %i", visibleLightCount);
    }

    void upload(std::vector<uniforms::point_light> & lights)
    {
        // Need a list of the index into the light list, and how many of them
        std::vector<std::pair<uint16_t, uint16_t>> lightListToSort;
        for (int i = 0; i < numLightIndices; ++i) lightListToSort.push_back({ lightClusterIDs[i], lightIndices[i] });
        std::sort(lightListToSort.begin(), lightListToSort.end());

        std::vector<uint16_t> packedLightIndices;

        // Indices are tightly packed
        uint16_t lastClusterID = 0;
        for (int i = 0; i < numLightIndices; ++i)
        {
            // We have the cluster ID and we know what light IDs are assigned to it
            uint16_t clusterId = lightListToSort[i].first;
            packedLightIndices.push_back(lightListToSort[i].second); // light index

            // New cluster
            if (clusterId != lastClusterID)
            {
                auto currentLightIndex = packedLightIndices.size(); // the last inserted
                clusterTable[clusterId].offset = currentLightIndex;
            }

            // Set the offset in the cluster table
            //clusterTable[lightListToSort[i].first].offset = currentLightIndex;

            //lastIndex = sortedIndexKey;
            lastClusterID = clusterId;
        }

        // repack the light indices now sorted by cluster id
        for (int i = 0; i < numLightIndices; ++i)
        {
            lightIndices[i] = packedLightIndices[i]; 
        }

        //if (sortedIndexKey != lastIndex)
       // {
       //     packedLightIndices.push_back(sortedIndexKey);
       // }

        /*
        for (auto & c : lightListToSort)
        {
            clusterTable[c.second].offset = 0;
            std::cout << "Sorted:  " << c.first  << ", " << c.second << std::endl;
        }
        std::cout << "====================================== \n";
        */
        /*
        // Offset + light count
        for (auto & c : clusterTable)
        {
            std::cout << "Offset " << c.offset << std::endl;
            std::cout << "Light Count: " << c.lightCount << std::endl;
        }
        std::cout << "====================================== \n";

        clusterTable[clusterId].lightCount += 1;
        clusterTable[clusterId].offset = ?;
        lightIndices[numLightIndices] = lightIndex;
        lightClusterIDs[numLightIndices] = clusterId;
        */

        // Update clustered lighting UBO
        glBindBufferBase(GL_UNIFORM_BUFFER, uniforms::clustered_lighting_buffer::binding, lightingBuffer);
        uniforms::clustered_lighting_buffer lighting = {};
        for (int l = 0; l < lights.size(); l++) lighting.lights[l] = lights[l];
        lightingBuffer.set_buffer_data(sizeof(lighting), &lighting, GL_STREAM_DRAW);
        gl_check_error(__FILE__, __LINE__);

        // Update Index Data
        glBindBuffer(GL_TEXTURE_BUFFER, lightIndexBuffer);
        lightIndexBuffer.set_buffer_data(sizeof(uint16_t) * lightIndices.size(), lightIndices.data(), GL_STREAM_DRAW); // fixme to use subData
        gl_check_error(__FILE__, __LINE__);

        // Update cluster grid
        glTextureSubImage3D(clusterTexture, 0, 0, 0, 0, NumClustersX, NumClustersY, NumClustersZ, GL_RG_INTEGER, GL_UNSIGNED_INT, (void *)clusterTable.data());
        gl_check_error(__FILE__, __LINE__);

        ImGui::Text("Uploaded %i lights indices to the lighting buffer", numLightIndices);
        ImGui::Text("Uploaded %i bytes to the index buffer", sizeof(uint16_t) * lightIndices.size());
    }

    std::vector<Frustum> build_froxels(const float4x4 & viewMatrix)
    {
        std::vector<Frustum> froxels;

        const float stepZ = (farClip - nearClip) / NumClustersZ;

        for (int z = 0; z < NumClustersZ; z++)
        {
            const float near = nearClip + (stepZ * z);
            const float far = near + stepZ;

            const float top = near * std::tan(vFov * 0.5f);       // normalized height
            const float right = top * aspect;                     // normalized width
            const float left = -right;
            const float bottom = -top;

            const float stepX = (right * 2.0f) / NumClustersX;
            const float stepY = (top   * 2.0f) / NumClustersY;

            float L, R, B, T;

            for (int y = 0; y < NumClustersY; y++)
            {
                for (int x = 0; x < NumClustersX; x++)
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

};

std::unique_ptr<ClusteredLighting> clusteredLighting;

shader_workbench::shader_workbench() : GLFWApp(1200, 800, "Clustered Shading Example")
{
    igm.reset(new gui::ImGuiInstance(window));

    gizmo.reset(new GlGizmo());
    xform.position = { 0.0f, 1.f, 0.0f };

    shaderMonitor.watch("../assets/shaders/wireframe_vert.glsl", "../assets/shaders/wireframe_frag.glsl", "../assets/shaders/wireframe_geom.glsl", [&](GlShader & shader)
    {
        wireframeShader = std::move(shader);
    });

    shaderMonitor.watch("../assets/shaders/prototype/simple_clustered_vert.glsl", "../assets/shaders/prototype/simple_clustered_frag.glsl", [&](GlShader & shader)
    {
        clusteredShader = std::move(shader);
    });

    grid.reset(new RenderableGrid(1.0f, 128, 128));

    basicShader = GlShader(default_color_vert, default_color_frag);

    sphereMesh = make_mesh_from_geometry(make_sphere(1.0f));
    floor = make_cube_mesh();

    for (int i = 0; i < 64; i++)
    {
        float4 randomPosition = float4(rand.random_float(-10, 10), rand.random_float(0.25, 0.25), rand.random_float(-10, 10), rand.random_float(0.5, 0.5)); // position + radius
        float4 randomColor = float4(rand.random_float(), rand.random_float(), rand.random_float(), 1.f);
        lights.push_back({ randomPosition, randomColor });
    }

    debugCamera.nearclip = 0.5f;
    debugCamera.farclip = 24.f;
    debugCamera.look_at({ 0, 3.0, -3.5 }, { 0, 2.0, 0 });
    cameraController.set_camera(&debugCamera);

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    clusteredLighting.reset(new ClusteredLighting(debugCamera.vfov, float(width) / float(height), debugCamera.nearclip, debugCamera.farclip));
}

shader_workbench::~shader_workbench() { }

void shader_workbench::on_window_resize(int2 size) { }

void shader_workbench::on_input(const InputEvent & event)
{
    cameraController.handle_input(event);
    if (igm) igm->update_input(event);
    if (gizmo) gizmo->handle_input(event);
}

void shader_workbench::on_update(const UpdateEvent & e)
{
    cameraController.update(e.timestep_ms);
    shaderMonitor.handle_recompile();
}

void shader_workbench::on_draw()
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (igm) igm->begin_frame();

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(0.2f, 0.2f, 0.2f, 0.0f);

    if (gizmo) gizmo->update(debugCamera, float2(width, height));
    tinygizmo::transform_gizmo("frustum", gizmo->gizmo_ctx, xform);

    const float windowAspectRatio = (float)width / (float)height;
    const float4x4 projectionMatrix = debugCamera.get_projection_matrix(windowAspectRatio);
    const float4x4 viewMatrix = debugCamera.get_view_matrix();
    const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);

    glViewport(0, 0, width, height);

    /*
    // Cluster Debugging
    {
        clusterCPUTimer.start();

        float4x4 debugViewMatrix = inverse(mul(make_translation_matrix({ xform.position.x, xform.position.y, xform.position.z }), make_scaling_matrix({ 1, 1, 1 })));
        float4x4 debugProjectionMatrix = projectionMatrix;

        draw_debug_frustum(&basicShader, mul(debugProjectionMatrix, debugViewMatrix), mul(projectionMatrix, viewMatrix), float4(1, 0, 0, 1));
        clusteredLighting->cull_lights(debugViewMatrix, debugProjectionMatrix, lights);

        auto froxelList = clusteredLighting->build_froxels(debugViewMatrix);
        for (int f = 0; f < froxelList.size(); f++)
        {
            float4 color = float4(1, 1, 1, .1f);

            Frustum frox = froxelList[f];
            if (clusteredLighting->clusterTable[f].lightCount > 0) color = float4(0.25, 0.35, .66, 1);
            draw_debug_frustum(&basicShader, frox, mul(projectionMatrix, viewMatrix), color);
        }

        clusterCPUTimer.pause();
    }
    */

    // Primary scene rendering
    {
        renderTimer.start();

        {
            clusteredShader.bind();

            clusteredLighting->cull_lights(viewMatrix, projectionMatrix, lights);

            clusteredLighting->upload(lights);

            clusteredShader.texture("s_clusterTexture", 0, clusteredLighting->clusterTexture, GL_TEXTURE_3D);
            clusteredShader.texture("s_lightIndexTexture", 1, clusteredLighting->lightIndexTexture, GL_TEXTURE_BUFFER);

            clusteredShader.uniform("u_eye", debugCamera.get_eye_point());
            clusteredShader.uniform("u_viewMat", viewMatrix); // hmm!
            clusteredShader.uniform("u_viewProj", viewProjectionMatrix);
            clusteredShader.uniform("u_diffuse", float3(1.0f, 1.0f, 1.0f));

            clusteredShader.uniform("u_nearClip", debugCamera.nearclip);
            clusteredShader.uniform("u_farClip", debugCamera.farclip);
            clusteredShader.uniform("u_rcpViewportSize", float2(1.f / (float) width, 1.f / (float) height));

            {
                float4x4 floorModel = make_scaling_matrix(float3(14, 0.1, 14));
                floorModel = mul(make_translation_matrix(float3(0, -0.1, 0)), floorModel);

                clusteredShader.uniform("u_modelMatrix", floorModel);
                clusteredShader.uniform("u_modelMatrixIT", inverse(transpose(floorModel)));
                floor.draw_elements();
            }

            clusteredShader.unbind();
        }

        renderTimer.stop();

        // Visualize the lights
        glDisable(GL_CULL_FACE);
        wireframeShader.bind();
        wireframeShader.uniform("u_eyePos", debugCamera.get_eye_point());
        wireframeShader.uniform("u_viewProjMatrix", viewProjectionMatrix);
        for (auto & l : lights)
        {
            auto translation = make_translation_matrix(l.positionRadius.xyz());
            auto scale = make_scaling_matrix(l.positionRadius.w);
            auto model = mul(translation, scale);
            wireframeShader.uniform("u_modelMatrix", model);
            sphereMesh.draw_elements();
        }
        glEnable(GL_CULL_FACE);
        wireframeShader.unbind();

        //grid->draw(viewProjectionMatrix);
    }
    
    if (gizmo) gizmo->draw();

    ImGui::Text("Render Time GPU %f ms", renderTimer.elapsed_ms());
    ImGui::Text("Cluster Generation CPU %u ms", clusterCPUTimer.milliseconds().count());

    if (igm) igm->end_frame();
    gl_check_error(__FILE__, __LINE__);

    glfwSwapBuffers(window);
}

IMPLEMENT_MAIN(int argc, char * argv[])
{
    try
    {
        shader_workbench app;
        app.main_loop();
    }
    catch (...)
    {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
