#include "index.hpp"
#include <iterator>
#include "svd.hpp"
#include "gl-gizmo.hpp"
#include "octree.hpp"

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


void draw_debug_frustum(GlShader * shader, const float4x4 & debugViewProjMatrix, const float4x4 & renderViewProjMatrix, const float4 & color)
{
    Frustum f(debugViewProjMatrix);
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


//  "2D Polyhedral Bounds of a Clipped, Perspective - Projected 3D Sphere"
void sphere_for_axis(const float3 & axis, const float3 & sphere_center, float sphere_radius, float znear, float3 boundsViewSpace[2])
{
    bool sphereClipByZNear = !((sphere_center.z + sphere_radius) < znear);

    // given in coordinates (a,z), where a is in the direction of the vector a, and z is in the standard z direction
    const float2 projectedCenter = float2(dot(axis, sphere_center), sphere_center.z);
    float2 bounds_az[2];

    float tSquared = dot(projectedCenter, projectedCenter) - (sphere_radius * sphere_radius);
    float t, cLength, costheta, sintheta;

    bool camera_outside_sphere = (tSquared > 0);
    if (camera_outside_sphere)
    { 
        // Distance to the tangent points of the sphere (points where a vector from the camera are tangent to the sphere) (calculated a-z space)
        t = sqrt(tSquared);
        cLength = length(projectedCenter);

        // Theta is the angle between the vector from the camera to the center of the sphere and the vectors from the camera to the tangent points
        costheta = t / cLength;
        sintheta = sphere_radius / cLength;
    }

    float sqrtPart;
    if (sphereClipByZNear)
    {
        sqrtPart = std::sqrt((sphere_radius * sphere_radius) - ((znear - projectedCenter.y) * (znear - projectedCenter.y)));
    }

    sqrtPart *= -1.f;
    for (int i = 0; i < 2; ++i)
    {
        if (tSquared >  0)
        {
            const float2x2 rotateTheta = { { costheta, -sintheta}, {sintheta, costheta} };

            float2 rotated = mul(rotateTheta, projectedCenter);
            float2 norm = rotated / length(rotated);
            float2 point = t * norm;

            bounds_az[i] = costheta * mul(rotateTheta, projectedCenter);

            int a = 0;
        }

        if (sphereClipByZNear && (!camera_outside_sphere || bounds_az[i].y > znear))
        {
            bounds_az[i].x = projectedCenter.x + sqrtPart;
            bounds_az[i].y = znear;
        }
        sintheta *= -1; 
        sqrtPart *= -1; 
    }

    boundsViewSpace[0] = bounds_az[0].x * axis;
    boundsViewSpace[0].z = bounds_az[0].y;
    boundsViewSpace[1] = bounds_az[1].x * axis;
    boundsViewSpace[1].z = bounds_az[1].y;
}


struct Light
{
    float4 positionRadius;
    float4 color;
};

// http://www.humus.name/Articles/PracticalClusteredShading.pdf
struct ClusteredLighting
{
    static const uint32_t NumClustersX = 16; // Tiles in X
    static const uint32_t NumClustersY = 8;  // Tiles in Y
    static const uint32_t NumClustersZ = 24; // Slices in Z

    float nearClip, farClip;
    float vFov;
    float aspect;

    // This is stored in a 3D texture (R32G32_UINT)
    struct ClusterPointer
    {
        uint32_t offset;        // offset into 
        uint32_t lightCount;
    };

    ClusterPointer clusterTable[NumClustersX * NumClustersY * NumClustersZ];

    // -1 to 1
    ClusteredLighting(float vFov, float aspect, float nearClip, float farClip) : vFov(vFov), aspect(aspect), nearClip(nearClip), farClip(farClip)
    {

    }

    void cull_lights(const float4x4 & viewMatrix, const float4x4 & projectionMatrix, const std::vector<Light> & lights)
    {   
        uint32_t visibleLightCount = 0;
        Frustum cameraFrustum(mul(projectionMatrix, viewMatrix));

        float nearFarDistanceRCP = 1.0f / (farClip - nearClip);

        for (int lightIndex = 0; lightIndex < lights.size(); ++lightIndex)
        {
            const Light & l = lights[lightIndex];

            // Conservative light culling based on worldspace camera frustum
            if (!cameraFrustum.intersects(l.positionRadius.xyz(), l.positionRadius.w))
            {
                continue;
            }

           visibleLightCount++;

           /*
           //float3 viewSpace = mul(viewMatrix, float4(l.positionRadius.xyz(), 0)).xyz();
           float linearDepthMin = (-viewSpace.z - l.positionRadius.w - nearClip) * nearFarDistanceRCP;
           float linearDepthMax = (-viewSpace.z + l.positionRadius.w - nearClip) * nearFarDistanceRCP;
           auto z0 = std::max(uint32_t(0), std::min((uint32_t) (linearDepthMin * (float) NumClustersZ), NumClustersZ - 1));
           auto z1 = std::max(uint32_t(0), std::min((uint32_t) (linearDepthMax * (float) NumClustersZ), NumClustersZ - 1));
           std::cout << "Z0 - " << z0 << ", Z1 - " << z1 << std::endl;
           */
           
           auto viewDepthToFroxelDepth = [&](float viewspaceDepth)
           {
               float vZ = (viewspaceDepth - nearClip) / (farClip - nearClip);
               float fZ = std::pow(vZ, 1 / 2.0f); // fixme, distribution factor 
               return clamp(fZ, 0.f, 1.f);
           };


           // Convert sphere to froxel bounds 
           float3 lightCenterVS = transform_coord(viewMatrix, l.positionRadius.xyz());
           float nearClipVS = -nearClip;

           float zLightMin = (-lightCenterVS.z) - l.positionRadius.w;
           float zLightMax = (-lightCenterVS.z) + l.positionRadius.w;

           float3 leftRightViewSpace[2];
           float3 bottomTopViewSpace[2];
           sphere_for_axis(float3(1, 0, 0), lightCenterVS, l.positionRadius.w, -nearClipVS, leftRightViewSpace);
           sphere_for_axis(float3(0, 1, 0), lightCenterVS, l.positionRadius.w, -nearClipVS, bottomTopViewSpace);

           Bounds3D result;
           result._min = float3(transform_coord(projectionMatrix, leftRightViewSpace[0]).x, transform_coord(projectionMatrix, bottomTopViewSpace[0]).y, viewDepthToFroxelDepth(zLightMin));
           result._max = float3(transform_coord(projectionMatrix, leftRightViewSpace[1]).x, transform_coord(projectionMatrix, bottomTopViewSpace[1]).y, viewDepthToFroxelDepth(zLightMax));

           //std::cout << "Left:    " << result.min().x << std::endl;
           //std::cout << "Bottom:  " << result.min().y << std::endl;
           //std::cout << "Right:   " << result.max().x << std::endl;
           //std::cout << "Top:     " << result.max().y << std::endl;

            float2 min = (result.min().xy() + float2(1.f)) * 0.5f; // screenspace? 
            float2 max = (result.max().xy() + float2(1.f)) * 0.5f;

           //std::cout << "center: " << result.center() << std::endl;
           //std::cout << "size:   " << result.size() << std::endl;

           result._min.x = min.x;
           result._min.y = min.y;
           result._max.x = max.x;
           result._max.y = max.y;

            // Clip space AABB
            //std::cout << result << std::endl;

            Bounds3D voxelsOverlappingSphere;
            voxelsOverlappingSphere._min.x = (int)clamp(floor(result._min.x * NumClustersX), 0.0f, NumClustersX - 1.0f);
            voxelsOverlappingSphere._min.y = (int)clamp(floor(result._min.y * NumClustersY), 0.0f, NumClustersY - 1.0f);
            voxelsOverlappingSphere._min.z = (int)clamp(floor(result._min.z * NumClustersZ), 0.0f, NumClustersZ - 1.0f);
            voxelsOverlappingSphere._max.x = (int)clamp(ceil(result._max.x  * NumClustersX), 0.0f, NumClustersX - 1.0f);
            voxelsOverlappingSphere._max.y = (int)clamp(ceil(result._max.y  * NumClustersY), 0.0f, NumClustersY - 1.0f);
            voxelsOverlappingSphere._max.z = (int)clamp(ceil(result._max.z  * NumClustersZ), 0.0f, NumClustersZ - 1.0f);

            voxelsOverlappingSphere._min.x = (int)voxelsOverlappingSphere._min.x / 2;
            voxelsOverlappingSphere._max.x = (int)voxelsOverlappingSphere._max.x / 2;
            voxelsOverlappingSphere._min.y = (int)voxelsOverlappingSphere._min.y / 2;
            voxelsOverlappingSphere._max.y = (int)voxelsOverlappingSphere._max.y / 2;

            for (int z = voxelsOverlappingSphere._min.z; z <= voxelsOverlappingSphere._max.z; z++)
            {
                for (int y = voxelsOverlappingSphere._min.y; y <= voxelsOverlappingSphere._max.y; y++)
                {
                    for (int x = voxelsOverlappingSphere._min.x; x <= voxelsOverlappingSphere._max.x; x++)
                    {

                        uint16_t clusterId = z * (NumClustersX * NumClustersY) + y * NumClustersX + x; 

                        // runtime assert max clusters

                        this->clusterLightPointerList[clusterId].pointLightCount++;

                        this->lightIndices[this->numLightIndices] = idx; // store light index
                        this->lightSortKeys[this->numLightIndices] = (clusterId); // &LIGHT_TYPE_MASK ); // store light key

                        ++this->numLightIndices;
                    }
                }
            }


        }

        ImGui::Text("Visible Lights %i", visibleLightCount);

        // Check if light is in frustum (sphere frustum check). 

        // Get the extents of the sphere in Z

        // Project the sphere, check planes

        // Add point to cluster

        // Sort light indices
    }

    std::vector<Frustum> build_froxels()
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

                    const float4x4 projection = make_projection_matrix(L, R, B, T, near, far);
                    const Frustum froxel(projection);
                    froxels.push_back(froxel);
                }
            }
        }

        return froxels;
    }

};

struct ExperimentalApp : public GLFWApp
{
    ShaderMonitor shaderMonitor = { "../assets/" };

    GlShader wireframeShader;
    GlShader basicShader;
    GlShader clusteredShader;

    std::vector<Light> lights;

    std::unique_ptr<gui::ImGuiInstance> igm;

    GlCamera debugCamera;
    FlyCameraController cameraController;
    std::unique_ptr<RenderableGrid> grid;

    UniformRandomGenerator rand;

    std::unique_ptr<GlGizmo> gizmo;
    tinygizmo::rigid_transform xform;

    GlMesh sphereMesh;
    GlMesh floor;
    GlGpuTimer gpuTimer;

    std::unique_ptr<ClusteredLighting> clusteredLighting;

    ExperimentalApp() : GLFWApp(1280, 800, "Nearly Empty App")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        gl_check_error(__FILE__, __LINE__);

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

        for (int i = 0; i < 2; i++)
        {
            float4 randomPosition = float4(rand.random_float(-10, 10), rand.random_float(0, 1), rand.random_float(-10, 10), rand.random_float(0.5, 0.5)); // position + radius
            float4 randomColor = float4(rand.random_float(), rand.random_float(), rand.random_float(), 1.f);
            lights.push_back({ randomPosition, randomColor });
        }

        debugCamera.nearclip = 1.f;
        debugCamera.farclip = 24.f;
        debugCamera.look_at({0, 3.0, -3.5}, {0, 2.0, 0});
        cameraController.set_camera(&debugCamera);

        clusteredLighting.reset(new ClusteredLighting(debugCamera.vfov, float(width) / float(height), debugCamera.nearclip, debugCamera.farclip));
    }
    
    void on_window_resize(int2 size) override
    {

    }
    
    void on_input(const InputEvent & event) override
    {
        cameraController.handle_input(event);
        if (igm) igm->update_input(event);
        if (gizmo) gizmo->handle_input(event);
    }
    
    void on_update(const UpdateEvent & e) override
    {
        cameraController.update(e.timestep_ms);
        shaderMonitor.handle_recompile();
    }
    
    void render_scene(const float4x4 & viewMatrix, const float4x4 & projectionMatrix)
    {
        gpuTimer.start();

        const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);

        const float4x4 debugProjection = make_perspective_matrix(1.f, 1.f, 0.5f, 12.f);
        Pose p = look_at_pose_rh(float3(0.00, -0.01, 0.00), float3(0, 0, -1.f));
        const float4x4 debugView = inverse(p.matrix());
        const float4x4 debugViewProj = mul(debugProjection, debugView);

        Frustum f(debugViewProj);
        //float3 color = (f.contains(float3(xform.position.x, xform.position.y, xform.position.z))) ? float3(1, 0, 0) : float3(0, 0, 0);
        //draw_debug_frustum(&basicShader, debugViewProj, viewProjectionMatrix, float3(1, 1, 1));
        //draw_debug_generated(&basicShader, viewProjectionMatrix, float3(16, 8, 24));
        //draw_debug_generated(&basicShader, 5.75, 12, viewProjectionMatrix);

        /*
        auto froxelList = clusteredLighting->build_froxels();
        for (auto & f : froxelList)
        {
            draw_debug_frustum(&basicShader, f, viewProjectionMatrix, float4(1.f, 1.f, 1.f, 0.5f));
        }
        */

        {
            clusteredShader.bind();

            clusteredShader.uniform("u_eye", debugCamera.get_eye_point());
            clusteredShader.uniform("u_viewProj", viewProjectionMatrix);
            clusteredShader.uniform("u_diffuse", float3(1.0f, 1.0f, 1.0f));

            for (int i = 0; i < lights.size(); i++)
            {
                clusteredShader.uniform("u_lights[" + std::to_string(i) + "].position", lights[i].positionRadius);
                clusteredShader.uniform("u_lights[" + std::to_string(i) + "].color", lights[i].color);
            }

            {
                float4x4 floorModel = make_scaling_matrix(float3(12, 0.1, 12));
                floorModel = mul(make_translation_matrix(float3(0, -0.1, 0)), floorModel);

                clusteredShader.uniform("u_modelMatrix", floorModel);
                clusteredShader.uniform("u_modelMatrixIT", inverse(transpose(floorModel)));
                floor.draw_elements();
            }

            clusteredShader.unbind();

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
        }

        //grid->draw(viewProjectionMatrix);

        gpuTimer.stop();

    }

    void on_draw() override
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
        tinygizmo::transform_gizmo("destination", gizmo->gizmo_ctx, xform);

        const float windowAspectRatio = (float)width / (float)height;
        const float4x4 projectionMatrix = debugCamera.get_projection_matrix(windowAspectRatio);
        const float4x4 viewMatrix = debugCamera.get_view_matrix();

        glViewport(0, 0, width, height);
        render_scene(viewMatrix, projectionMatrix);

        //float4x4 debugViewMatrix = inverse(mul(make_translation_matrix({ xform.position.x, xform.position.y, xform.position.z }), make_scaling_matrix({ 1, 2, 1 })));
        //draw_debug_frustum(&basicShader, debugViewMatrix, mul(projectionMatrix, viewMatrix), float4(1, 0, 0, 1));

        //std::cout << debugViewMatrix << std::endl;

        clusteredLighting->cull_lights(viewMatrix, projectionMatrix, lights);

        if (gizmo) gizmo->draw();

        ImGui::Text("Render Time %f ms", gpuTimer.elapsed_ms());
        if (igm) igm->end_frame();
        gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
    }
    
};