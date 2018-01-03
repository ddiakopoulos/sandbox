#include "index.hpp"
#include <random>

constexpr const char basic_vert[] = R"(#version 330
    layout(location = 0) in vec3 vertex;
    layout(location = 2) in vec3 inColor;
    uniform mat4 u_mvp;
    out vec3 color;
    void main()
    {
        gl_Position = u_mvp * vec4(vertex.xyz, 1);
        color = inColor;
    }
)";

constexpr const char basic_frag[] = R"(#version 330
    in vec3 color;
    out vec4 f_color;
    uniform vec3 u_color;
    void main()
    {
        f_color = vec4(color, 1);
    }
)";

struct CameraPathFollower
{
    std::vector<float4x4> parallelTransportFrames;

    void compute(std::array<Pose, 4> & controlPoints)
    {
        parallelTransportFrames = make_parallel_transport_frame_bezier(controlPoints, 128);
    }

    float4x4 get_transform(const size_t idx) const
    {
        assert(parallelTransportFrames.size() > 0);
        if (idx >= 0) return parallelTransportFrames[idx];
        else return parallelTransportFrames[parallelTransportFrames.size()];
    }

    std::vector<GlMesh> debug_draw() const
    {
        assert(parallelTransportFrames.size() > 0);
        std::vector<GlMesh> axisMeshes;

        for (int i = 0; i < parallelTransportFrames.size(); ++i)
        {
            auto m = get_transform(i);

            float3 qxdir = mul(m, float4(1, 0, 0, 0)).xyz();
            float3 qydir = mul(m, float4(0, 1, 0, 0)).xyz();
            float3 qzdir = mul(m, float4(0, 0, 1, 0)).xyz();

            auto axisMesh = make_axis_mesh(qxdir, qydir, qzdir);
            axisMeshes.push_back(std::move(axisMesh));
        }
        return axisMeshes;
    }

    void reset()
    {
        parallelTransportFrames.clear();
    }
};

using namespace gui;

struct ExperimentalApp : public GLFWApp
{
    uint64_t frameCount = 0;

    GlCamera camera;
    
    HosekProceduralSky skydome;
    FlyCameraController cameraController;

    std::unique_ptr<imgui_wrapper> igm;
    std::unique_ptr<GlGizmo> gizmo;

    tinygizmo::rigid_transform destination;

    std::shared_ptr<GlShader> basicShader;
    std::unique_ptr<GlShader> terrainShader;
    std::unique_ptr<GlShader> waterShader;
    
    GlMesh axisMesh, sphereMesh;

    GlFramebuffer reflectionFramebuffer;
    GlTexture2D sceneColorTexture;
    
    GlFramebuffer depthFramebuffer;
    GlTexture2D sceneDepthTexture;
    
    GlMesh waterMesh;
    GlMesh terrainMesh;
    GlMesh icosahedronMesh;

    const float clipPlaneOffset = 0.075f;
    
    float yWaterPlane = 0.0f;
    int yIndex = 0;
    float4x4 terrainTranslationMat = make_translation_matrix({-16, static_cast<float>(yIndex), -16});
    
    float appTime = 0;
    float rotationAngle = 0.0f;
    
    UniformRandomGenerator generator;

    CameraPathFollower follower;
    bool cameraFollowing = false;
    int playbackIndex = 0;

    int splinePointIndex = 0;
    std::array<Pose, 4> cameraSpline;

    ExperimentalApp() : GLFWApp(1280, 720, "Terrain & Water Scene")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        camera.look_at({0, 4, 12}, {0, 0.0f, -0.1f});

        cameraController.set_camera(&camera);

        basicShader = std::make_shared<GlShader>(basic_vert, basic_frag);

        terrainShader.reset(new GlShader(read_file_text("../assets/shaders/prototype/terrain_vert_debug.glsl"), read_file_text("../assets/shaders/prototype/terrain_frag_debug.glsl")));
        waterShader.reset(new GlShader(read_file_text("../assets/shaders/prototype/water_vert.glsl"), read_file_text("../assets/shaders/prototype/water_frag.glsl")));
        
        sceneColorTexture.setup(width, height, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glNamedFramebufferTexture2DEXT(reflectionFramebuffer, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneColorTexture, 0);
        reflectionFramebuffer.check_complete();
        
        sceneDepthTexture.setup(width, height, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glNamedFramebufferTexture2DEXT(depthFramebuffer, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, sceneDepthTexture, 0);
        depthFramebuffer.check_complete();

        gl_check_error(__FILE__, __LINE__);

        waterMesh = make_plane_mesh(112.f, 112.f, 256, 256);
        terrainMesh = make_mesh_from_geometry(make_perlin_mesh(64, 64));
        icosahedronMesh = make_icosahedron_mesh();
        
        gizmo.reset(new GlGizmo());
        igm.reset(new gui::imgui_wrapper(window));
        gui::make_light_theme();

        sphereMesh = make_sphere_mesh(0.1f);
        axisMesh = make_axis_mesh();

        gl_check_error(__FILE__, __LINE__);
    }
    
    std::vector<float> make_radial_mask(int size, float heightScale = 1.0)
    {
        float radius = size / 2.0f;
        std::vector<float> mask(size * size * 2);
        
        for (int iy = 0; iy <= size; iy++)
        {
            for (int ix = 0; ix <= size; ix++)
            {
                auto n = generator.random_float(-1.f, 1.f);
                float centerToX = (ix + n) - radius;
                n = generator.random_float(0.f, 3.f);
                float centerToY = (iy + n) - radius;
                float distance = (float) sqrt(centerToX * centerToX + centerToY * centerToY);
                float delta = distance / (radius);
                mask[iy * size + ix] = (delta * delta);
            }
        }
        
        return mask;
    }
    
    Geometry make_perlin_mesh(int width, int height)
    {
        Geometry terrain;
        float gridSize = 32.0f;
        
        auto mask = make_radial_mask(32);
        
        for (int x = 0; x <= gridSize; x++)
        {
            for (int z = 0; z <= gridSize; z++)
            {
                float y = ((noise::noise(float2(x * 0.1f, z * 0.1f))) + 1.0f) / 2.0f;
                y = y * 10.0f;
                //float w = 0.54 - 0.46 * cos(ANVIL_TAU * (x * gridSize + z) / ( gridSize));
                auto w = mask[x * gridSize + z];
                terrain.vertices.push_back({(float)x, y * (1 - w), (float)z});
            }
        }
        
        std::vector<uint4> quads;
        for(int x = 0; x < gridSize; ++x)
        {
            for(int z = 0; z < gridSize; ++z)
            {
                std::uint32_t tlIndex = z * (gridSize+1) + x;
                std::uint32_t trIndex = z * (gridSize+1) + (x + 1);
                std::uint32_t blIndex = (z + 1) * (gridSize+1) + x;
                std::uint32_t brIndex = (z + 1) * (gridSize+1) + (x + 1);
                quads.push_back({blIndex, tlIndex, trIndex, brIndex});
            }
        }
        
        for (auto f : quads)
        {
            terrain.faces.push_back(uint3(f.x, f.y, f.z));
            terrain.faces.push_back(uint3(f.x, f.z, f.w));
            //terrain.faces.push_back(uint3(f.z, f.y, f.x));
            //terrain.faces.push_back(uint3(f.w, f.z, f.x));
        }
        
        terrain.compute_normals();
        return terrain;
    }
    
    void on_window_resize(int2 size) override
    {

    }
    
    void on_input(const InputEvent & event) override
    {
        gizmo->handle_input(event);
        igm->update_input(event);
        cameraController.handle_input(event);
        
        if (event.type == InputEvent::KEY && event.action == GLFW_RELEASE)
        {
            if (event.value[0] == GLFW_KEY_1)
            {
                terrainTranslationMat = make_translation_matrix({0, static_cast<float>(yIndex++), 0});
            }
            else if (event.value[0] == GLFW_KEY_2)
            {
                terrainTranslationMat = make_translation_matrix({ 0, static_cast<float>(yIndex--), 0 });
            }
            else if (event.value[0] == GLFW_KEY_SPACE)
            {
                cameraSpline[splinePointIndex++] = camera.get_pose();
                if (splinePointIndex == 4) follower.compute(cameraSpline);
            }
        }
    }
    
    void on_update(const UpdateEvent & e) override
    {
        appTime = e.elapsed_s;
        cameraController.update(e.timestep_ms);
        rotationAngle += e.timestep_ms;
    }
    
    void draw_terrain(float3 cameraPosition, float4x4 viewMatrix)
    {
        glEnable(GL_BLEND);
        
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        
        terrainShader->bind();
        
        terrainShader->uniform("u_eyePosition", cameraPosition);
        terrainShader->uniform("u_lightPosition", float3(0.0, 10.0, 0.0));
        terrainShader->uniform("u_clipPlane", float4(0, 0, 0, 0));
        
		float4x4 viewProj = mul(camera.get_projection_matrix((float)width / (float)height), viewMatrix);

        {
            float4x4 model = terrainTranslationMat;
            float4x4 mvp = mul(camera.get_projection_matrix((float) width / (float) height), viewMatrix, model);
            float4x4 modelViewMat = mul(viewMatrix, model);
            
            terrainShader->uniform("u_mvp", mvp);
			terrainShader->uniform("u_model", model);
			terrainShader->uniform("u_viewProj", viewProj);
            terrainShader->uniform("u_modelView", modelViewMat);
            terrainShader->uniform("u_modelMatrixIT", get_rotation_submatrix(inverse(transpose(modelViewMat))));
            terrainShader->uniform("u_surfaceColor", float3(95.f / 255.f, 189.f / 255.f, 192.f / 255.f));
            terrainMesh.draw_elements();
        }
        
        {
            float4x4 model = mul(Identity4x4, make_translation_matrix({0, 12, 0}), make_rotation_matrix({0, 1, 0}, rotationAngle * 0.99f));
            float4x4 mvp = mul(camera.get_projection_matrix((float) width / (float) height), viewMatrix, model);
            float4x4 modelViewMat = mul(viewMatrix, model);
            
            terrainShader->uniform("u_mvp", mvp);
			terrainShader->uniform("u_model", model);
			terrainShader->uniform("u_viewProj", viewProj);
            terrainShader->uniform("u_modelView", modelViewMat);
            terrainShader->uniform("u_modelMatrixIT", get_rotation_submatrix(inverse(transpose(modelViewMat))));
            terrainShader->uniform("u_surfaceColor", float3(189.f / 255.f, 94.f / 255.f, 188.f / 255.f));
            icosahedronMesh.draw_elements();
        }
        
        terrainShader->unbind();
        
        glDisable(GL_BLEND);
        
        gl_check_error(__FILE__, __LINE__);
    }
    
    // Given position/normal of the plane, calculates plane in camera space.
    float4 camera_space_plane(float4x4 viewMatrix, float3 pos, float3 normal, float sideSign, float clipPlaneOffset)
    {
        float3 offsetPos = pos + normal * clipPlaneOffset;
        float4x4 m = viewMatrix;
        float3 cpos = transform_coord(m, offsetPos);
        float3 cnormal = normalize(transform_vector(m, normal)) * sideSign;
        return float4(cnormal.x, cnormal.y, cnormal.z, -dot(cpos,cnormal));
    }
    
    void on_draw() override
    {
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);

        int width, height;
        glfwGetWindowSize(window, &width, &height);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.f, 0.0f, 0.0f, 1.0f);

        if (gizmo) gizmo->update(camera, float2(width, height));
        tinygizmo::transform_gizmo("destination", gizmo->gizmo_ctx, destination);

        float4x4 viewMatrix = camera.get_view_matrix();
        float3 cameraPosition = camera.get_eye_point();

        /*
        // Loop the playback if following
        if (cameraFollowing && playbackIndex == follower.parallelTransportFrames.size())
        {
            playbackIndex = 0;
        }
        */

        // Set view matrix
        if (cameraFollowing) viewMatrix = follower.get_transform(playbackIndex);
        else viewMatrix = camera.get_view_matrix();

        // Correct for inverse
        if (cameraFollowing)
        {
            viewMatrix = inverse(viewMatrix);
        }

        // Set position
        if (cameraFollowing) cameraPosition = follower.get_transform(playbackIndex)[3].xyz();
        else cameraPosition = camera.get_eye_point();

        const float4x4 viewProj = mul(camera.get_projection_matrix((float)width / (float)height), viewMatrix);

        skydome.render(viewProj, cameraPosition, camera.farclip);

        {
            // Wind in reverse order for reflection
            glFrontFace(GL_CW);

            glBindFramebuffer(GL_FRAMEBUFFER, reflectionFramebuffer);
            glViewport(0, 0, width, height);

            glClear(GL_COLOR_BUFFER_BIT);
            glClearColor(0.f, 0.0f, 0.0f, 1.0f);

            // Reflect camera around reflection plane
            float3 normal = float3(0, 1, 0);
            float3 pos = { 0, 0, 0 }; // Location of object... here, the terrain
            float4 reflectionPlane = float4(normal.x, normal.y, normal.z, 0.f);

            float4x4 model = mul(Identity4x4, terrainTranslationMat);

            // Take position, transform into world space with model, reflect about a world space reflection plane, then take it into the view space of the camera
            // ViewProj takes the camera local coordinate into screen coordinates (3D to the 2D + Z)
            // gl_position = proj * view * refl * model * position;

            terrainShader->bind();
            terrainShader->uniform("u_viewProj", mul(viewProj, make_reflection_matrix(reflectionPlane)));
            terrainShader->uniform("u_model", model);
            terrainShader->uniform("u_eyePosition", cameraPosition);
            terrainShader->uniform("u_clipPlane", reflectionPlane);
            terrainShader->uniform("u_lightPosition", float3(0.0, 10.0, 0.0));
            terrainShader->uniform("u_surfaceColor", float3(1, 1, 1));
            terrainMesh.draw_elements();

            terrainShader->unbind();

            // Pop reverse winding
            glFrontFace(GL_CCW);
        }

        {
            glBindFramebuffer(GL_FRAMEBUFFER, depthFramebuffer);
            glViewport(0, 0, width, height);

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

            draw_terrain(cameraPosition, viewMatrix);

            gl_check_error(__FILE__, __LINE__);
        }

        // Output to default framebuffer
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, width, height);

            draw_terrain(cameraPosition, viewMatrix);

            // Draw Water
            {
                // Gives it a bit of a wispy look....
                //glEnable(GL_BLEND);
                //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                float4x4 model = make_rotation_matrix({ 1, 0, 0 }, -ANVIL_PI / 2);

                auto mvp = mul(camera.get_projection_matrix((float)width / (float)height), viewMatrix, model);
                float4x4 modelViewMat = mul(viewMatrix, model);

                waterShader->bind();

                waterShader->uniform("u_mvp", mvp);
                waterShader->uniform("u_time", appTime);
                waterShader->uniform("u_yWaterPlane", yWaterPlane);
                waterShader->uniform("u_eyePosition", cameraPosition);
                waterShader->uniform("u_modelView", modelViewMat);
                waterShader->uniform("u_modelMatrixIT", get_rotation_submatrix(inverse(transpose(model))));
                waterShader->uniform("u_resolution", float2(width, height));

                waterShader->uniform("u_clipPlane", { 0, 1, 0, 0 });

                waterShader->texture("u_reflectionTexture", 0, sceneColorTexture.id(), GL_TEXTURE_2D);
                waterShader->texture("u_depthTexture", 1, sceneDepthTexture.id(), GL_TEXTURE_2D);

                waterShader->uniform("u_near", camera.nearclip);
                waterShader->uniform("u_far", camera.farclip);
                waterShader->uniform("u_lightPosition", float3(0.0, 10.0, 0.0));

                waterMesh.draw_elements();
                waterShader->unbind();

                //glDisable(GL_BLEND);
            }

            {
                basicShader->bind();

                // Debug draw crosses
                if (follower.parallelTransportFrames.size() > 0)
                {
                    auto debugMeshes = follower.debug_draw();
                    for (int i = 0; i < follower.parallelTransportFrames.size(); ++i)
                    {
                        auto modelMatrix = follower.get_transform(i);
                        basicShader->uniform("u_mvp", mul(viewProj, make_translation_matrix(modelMatrix[3].xyz()))); // no model transformation
                        debugMeshes[i].draw_elements(); // axes drawn directly from matrix
                    }
                }

                // Draw the camera control points
                for (auto & m : cameraSpline)
                {
                    basicShader->uniform("u_mvp", mul(viewProj, make_translation_matrix(m.position)));
                    sphereMesh.draw_elements();
                }

                basicShader->unbind();
            }

        }

        gl_check_error(__FILE__, __LINE__);
        
        igm->begin_frame();
        ImGui::SliderInt("Playback Index", &playbackIndex, 0, follower.parallelTransportFrames.size());
        ImGui::Checkbox("Follow", &cameraFollowing);
        igm->end_frame();

        glfwSwapBuffers(window);
        
        frameCount++;
    }
    
};
