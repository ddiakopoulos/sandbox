#include "index.hpp"
#include <random>

struct ExperimentalApp : public GLFWApp
{
    uint64_t frameCount = 0;

    GlCamera camera;
    
    HosekProceduralSky skydome;
    FlyCameraController cameraController;

    std::unique_ptr<GlShader> terrainShader;
    std::unique_ptr<GlShader> waterShader;
    
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
    
    std::random_device rd;
    std::mt19937 gen;
    
    ExperimentalApp() : GLFWApp(1280, 720, "Terrain & Water Scene")
    {
        gen = std::mt19937(rd());
        
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        camera.farclip = 96;
        camera.look_at({0, 4, 12}, {0, 0.1f, 0});

        cameraController.set_camera(&camera);

        terrainShader.reset(new GlShader(read_file_text("../assets/shaders/terrain_vert_debug.glsl"), read_file_text("../assets/shaders/terrain_frag_debug.glsl")));
        waterShader.reset(new GlShader(read_file_text("../assets/shaders/water_vert.glsl"), read_file_text("../assets/shaders/water_frag.glsl")));
        
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
                auto n = std::uniform_real_distribution<float>(-1.0f, 1.0f)(gen);
                float centerToX = (ix + n) - radius;
                n = std::uniform_real_distribution<float>(0.0f, 3.0f)(gen);
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
        cameraController.handle_input(event);
        
        if (event.type == InputEvent::KEY && event.action == GLFW_RELEASE)
        {
            if (event.value[0] == GLFW_KEY_1)
                terrainTranslationMat = make_translation_matrix({0, static_cast<float>(yIndex++), 0});
            else if (event.value[0] == GLFW_KEY_2)
                terrainTranslationMat = make_translation_matrix({0, static_cast<float>(yIndex--), 0});
        }
    }
    
    void on_update(const UpdateEvent & e) override
    {
        appTime = e.elapsed_s;
        cameraController.update(e.timestep_ms);
        rotationAngle += e.timestep_ms;
    }
    
    void draw_terrain()
    {
        glEnable(GL_BLEND);
        
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        
        terrainShader->bind();
        
        terrainShader->uniform("u_eyePosition", camera.get_eye_point());
        terrainShader->uniform("u_lightPosition", float3(0.0, 10.0, 0.0));
        terrainShader->uniform("u_clipPlane", float4(0, 0, 0, 0));
        
		float4x4 viewProj = mul(camera.get_projection_matrix((float)width / (float)height), camera.get_view_matrix());

        {
            float4x4 model = terrainTranslationMat;
            float4x4 mvp = mul(camera.get_projection_matrix((float) width / (float) height), camera.get_view_matrix(), model);
            float4x4 modelViewMat = mul(camera.get_view_matrix(), model);
            
            terrainShader->uniform("u_mvp", mvp);
			terrainShader->uniform("u_model", model);
			terrainShader->uniform("u_viewProj", viewProj);
            terrainShader->uniform("u_modelView", modelViewMat);
            terrainShader->uniform("u_modelMatrixIT", get_rotation_submatrix(inv(transpose(modelViewMat))));
            terrainShader->uniform("u_surfaceColor", float3(95.f / 255.f, 189.f / 255.f, 192.f / 255.f));
            terrainMesh.draw_elements();
        }
        
        {
            float4x4 model = mul(Identity4x4, make_translation_matrix({0, 12, 0}), make_rotation_matrix({0, 1, 0}, rotationAngle * 0.99f));
            float4x4 mvp = mul(camera.get_projection_matrix((float) width / (float) height), camera.get_view_matrix(), model);
            float4x4 modelViewMat = mul(camera.get_view_matrix(), model);
            
            terrainShader->uniform("u_mvp", mvp);
			terrainShader->uniform("u_model", model);
			terrainShader->uniform("u_viewProj", viewProj);
            terrainShader->uniform("u_modelView", modelViewMat);
            terrainShader->uniform("u_modelMatrixIT", get_rotation_submatrix(inv(transpose(modelViewMat))));
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

        const float4x4 viewProj = mul(camera.get_projection_matrix((float)width / (float)height), camera.get_view_matrix());

        skydome.render(viewProj, camera.get_eye_point(), camera.farclip);

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
            terrainShader->uniform("u_eyePosition", camera.get_eye_point());
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

            draw_terrain();

            gl_check_error(__FILE__, __LINE__);
        }

        // Output to default framebuffer
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, width, height);

            draw_terrain();

            // Draw Water
            {
                // Gives it a bit of a wispy look....
                //glEnable(GL_BLEND);
                //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                float4x4 model = make_rotation_matrix({ 1, 0, 0 }, -ANVIL_PI / 2);

                auto mvp = mul(camera.get_projection_matrix((float)width / (float)height), camera.get_view_matrix(), model);
                float4x4 modelViewMat = mul(camera.get_view_matrix(), model);

                waterShader->bind();

                waterShader->uniform("u_mvp", mvp);
                waterShader->uniform("u_time", appTime);
                waterShader->uniform("u_yWaterPlane", yWaterPlane);
                waterShader->uniform("u_eyePosition", camera.get_eye_point());
                waterShader->uniform("u_modelView", modelViewMat);
                waterShader->uniform("u_modelMatrixIT", get_rotation_submatrix(inv(transpose(model))));
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

        }

        gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
        
        frameCount++;
    }
    
};
