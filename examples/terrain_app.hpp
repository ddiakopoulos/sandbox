#include "index.hpp"
#include "noise.h"
#include <random>

using namespace math;
using namespace util;
using namespace gfx;

struct ExperimentalApp : public GLFWApp
{
    uint64_t frameCount = 0;

    GlCamera camera;
    
    HosekProceduralSky skydome;
    FPSCameraController cameraController;

    std::unique_ptr<GlShader> terrainShader;
    std::unique_ptr<GlShader> waterShader;
    
    GlFramebuffer reflectionFramebuffer;
    GlTexture sceneColorTexture;
    
    GlFramebuffer depthFramebuffer;
    GlTexture sceneDepthTexture;
    
    Renderable waterMesh;
    Renderable terrainMesh;
    Renderable icosahedron;
    
    std::unique_ptr<GLTextureView> colorTextureView;
    std::unique_ptr<GLTextureView> depthTextureView;
    
    const float clipPlaneOffset = 0.075f;
    
    float yWaterPlane = 0.0f;
    int yIndex = 0;
    float4x4 terrainTranslationMat = make_translation_matrix({-16, static_cast<float>(yIndex), -16});
    
    UWidget rootWidget;
    
    float appTime = 0;
    float rotationAngle = 0.0f;
    
    std::random_device rd;
    std::mt19937 gen;
    
    ExperimentalApp() : GLFWApp(940, 720, "Sandbox App")
    {
        gen = std::mt19937(rd());
        
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        cameraController.set_camera(&camera);
        
        camera.look_at({0, 4, 12}, {0, 0, 0});

        terrainShader.reset(new gfx::GlShader(read_file_text("assets/shaders/terrain_vert_debug.glsl"), read_file_text("assets/shaders/terrain_frag_debug.glsl")));
        waterShader.reset(new gfx::GlShader(read_file_text("assets/shaders/water_vert.glsl"), read_file_text("assets/shaders/water_frag.glsl")));
        
        sceneColorTexture.load_data(width, height, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        reflectionFramebuffer.attach(GL_COLOR_ATTACHMENT0, sceneColorTexture);
        if (!reflectionFramebuffer.check_complete()) throw std::runtime_error("incomplete framebuffer");
        
        sceneDepthTexture.load_data(width, height, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        depthFramebuffer.attach(GL_DEPTH_ATTACHMENT, sceneDepthTexture);
        if (!depthFramebuffer.check_complete()) throw std::runtime_error("incomplete framebuffer");
        
        gfx::gl_check_error(__FILE__, __LINE__);

        waterMesh = Renderable(make_plane(112.f, 112.f, 256, 256));
        
        // Seed perlin noise
        seed((float) std::uniform_int_distribution<int>(0, 512)(gen));
        terrainMesh = make_perlin_mesh(64, 64); //Renderable(make_cube());
        
        icosahedron = Renderable(make_icosahedron());
        
        colorTextureView.reset(new GLTextureView(sceneColorTexture.get_gl_handle()));
        depthTextureView.reset(new GLTextureView(sceneDepthTexture.get_gl_handle()));
        
        gfx::gl_check_error(__FILE__, __LINE__);
        
        // Set up the UI
        rootWidget.bounds = {0, 0, (float) width, (float) height};
        rootWidget.add_child( {{0,+10},{0,+10},{0.25,0},{0.25,0}}, std::make_shared<UWidget>()); // for colorTexture
        rootWidget.add_child( {{.25,+10},{0, +10},{0.50, -10},{0.25,0}}, std::make_shared<UWidget>()); // for depthTexture
        rootWidget.layout();
    }
    
    std::vector<float> make_radial_mask(int size, float heightScale = 1.0)
    {
        float radius = size / 2.0f;
        std::vector<float> mask(size * size + 2);
        
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
                //const float sigma = 1024.0f;
                float y = simplex2(x * 0.02f, z * 0.01f, 4.0f, 4.0f, 2.0f) * 12.5;
                //float w = 0.54 - 0.46 * cos(ANVIL_TAU * (x * gridSize + z) / ( gridSize));
                auto w = mask[x * gridSize + z];
                std::cout << w << std::endl;
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
    
    void on_window_resize(math::int2 size) override
    {

    }
    
    void on_input(const InputEvent & event) override
    {
        cameraController.handle_input(event);
        
        if (event.type == InputEvent::KEY)
        {
            if (event.value[0] == GLFW_KEY_1 && event.action == GLFW_RELEASE)
                terrainTranslationMat = make_translation_matrix({0, static_cast<float>(yIndex++), 0});
            else if (event.value[0] == GLFW_KEY_2 && event.action == GLFW_RELEASE)
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

        std::vector<float4x4> models;
        models.push_back(terrainTranslationMat);
        
        terrainShader->uniform("u_eyePosition", camera.get_eye_point());
        terrainShader->uniform("u_lightPosition", float3(0.0, 10.0, 0.0));
        terrainShader->uniform("u_clipPlane", float4(0, 0, 0, 0));
        
        for (auto m : models)
        {
            float4x4 model = Identity4x4 * m;
            float4x4 mvp = camera.get_projection_matrix((float) width / (float) height) * camera.get_view_matrix() * model;
            float4x4 modelViewMat = camera.get_view_matrix() * model;
            
            terrainShader->uniform("u_mvp", mvp);
            terrainShader->uniform("u_modelView", modelViewMat);
            terrainShader->uniform("u_modelMatrixIT", get_rotation_submatrix(inv(transpose(modelViewMat))));
            terrainShader->uniform("u_surfaceColor", float3(95.f / 255.f, 189.f / 255.f, 192.f / 255.f));
            terrainMesh.draw();
        }
        
        {
            float4x4 model = Identity4x4 * make_translation_matrix({0, 12, 0}) * make_rotation_matrix({0, 1, 0}, rotationAngle * 0.99f);
            float4x4 mvp = camera.get_projection_matrix((float) width / (float) height) * camera.get_view_matrix() * model;
            float4x4 modelViewMat = camera.get_view_matrix() * model;
            
            terrainShader->uniform("u_mvp", mvp);
            terrainShader->uniform("u_modelView", modelViewMat);
            terrainShader->uniform("u_modelMatrixIT", get_rotation_submatrix(inv(transpose(modelViewMat))));
            terrainShader->uniform("u_surfaceColor", float3(189.f / 255.f, 94.f / 255.f, 188.f / 255.f));
            icosahedron.draw();
        }
        
        terrainShader->unbind();
        
        glDisable(GL_BLEND);
        
        gfx::gl_check_error(__FILE__, __LINE__);
    }
    
    void draw_ui()
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        //colorTextureView->draw(rootWidget.children[0]->bounds, int2(width, height));
        //depthTextureView->draw(rootWidget.children[1]->bounds, int2(width, height));
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
        glfwMakeContextCurrent(window);
        
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);

        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.f, 0.0f, 0.0f, 1.0f);
        
        float4x4 viewProj = camera.get_projection_matrix((float) width / (float) height) * camera.get_view_matrix();
        
        skydome.render(viewProj, camera.get_eye_point(), camera.farClip);
        
        {
            // Wind in reverse order
            glFrontFace(GL_CW);
            
            reflectionFramebuffer.bind_to_draw();
            glClear(GL_COLOR_BUFFER_BIT);
            glClearColor(0.f, 0.0f, 0.0f, 1.0f);
            
            Pose savedCameraPose = camera.pose;
            
            // Set up temporary reflection camera
            {
                float3 newPosition = camera.pose.position;
                newPosition.y *= -1.0f;
                camera.set_position(newPosition); // newPosition
                
                // Flip X axis
                auto e = make_euler_from_quat(camera.pose.orientation);
                camera.set_orientation(make_quat_from_euler(-e.x, e.y, e.z));
            }
            
            // Reflect camera around reflection plane
            float3 normal = float3(0, 1, 0);
            float3 pos = {0, 0, 0}; // Location of object... here, the terrain
            float d = -dot(normal, pos) - clipPlaneOffset;
            float4 reflectionPlane = float4(normal.x, normal.y, normal.z, d);
            
            float4 clipPlane = camera_space_plane(camera.get_view_matrix(), pos, normal, 1.0f, clipPlaneOffset);
            
            float4x4 reflection = make_reflection_matrix(reflectionPlane);
            
            float4x4 reflectedView = reflection * camera.get_view_matrix();

            float4x4 model = Identity4x4 * terrainTranslationMat;
            float4x4 mvp = camera.get_projection_matrix((float) width / (float) height) * reflectedView * model;
            float4x4 modelViewMat = reflectedView * model;
            
            terrainShader->bind();
            terrainShader->uniform("u_mvp", mvp);
            terrainShader->uniform("u_modelView", modelViewMat);
            terrainShader->uniform("u_eyePosition", camera.get_eye_point());
            terrainShader->uniform("u_modelMatrixIT", get_rotation_submatrix(inv(transpose(modelViewMat))));
            terrainShader->uniform("u_clipPlane", clipPlane);
            terrainShader->uniform("u_lightPosition", float3(0.0, 10.0, 0.0));
            terrainShader->uniform("u_surfaceColor", float3(1, 1, 1));
            terrainMesh.draw();
            
            terrainShader->unbind();
            
            reflectionFramebuffer.unbind();
            
            camera.pose = savedCameraPose;
            
            // Pop reverse winding
            glFrontFace(GL_CCW);
        }
        
        {
            depthFramebuffer.bind_to_draw();
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
            draw_terrain();
            gfx::gl_check_error(__FILE__, __LINE__);
            depthFramebuffer.unbind();
        }
        
         draw_terrain();
        
        // Draw Water
        {
            // Gives it a bit of a wispy look....
            // glEnable(GL_BLEND);
            // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            float4x4 model = make_rotation_matrix({1, 0, 0}, ANVIL_PI / 2);
            auto mvp = camera.get_projection_matrix((float) width / (float) height) * camera.get_view_matrix() * model;
            float4x4 modelViewMat = camera.get_view_matrix() * model;
            
            waterShader->bind();
            
            waterShader->uniform("u_mvp", mvp);
            waterShader->uniform("u_time", appTime);
            waterShader->uniform("u_yWaterPlane", yWaterPlane);
            waterShader->uniform("u_eyePosition", camera.get_eye_point());
            waterShader->uniform("u_modelView", modelViewMat);
            waterShader->uniform("u_modelMatrixIT", get_rotation_submatrix(inv(transpose(modelViewMat))));
            waterShader->uniform("u_resolution", float2(width, height));
            
            waterShader->uniform("u_clipPlane", {0, 1, 0, 0});
            
            waterShader->texture("u_reflectionTexture", 0, sceneColorTexture.get_gl_handle(), GL_TEXTURE_2D);
            waterShader->texture("u_depthTexture", 1, sceneDepthTexture.get_gl_handle(), GL_TEXTURE_2D);
            
            waterShader->uniform("u_near", camera.nearClip);
            waterShader->uniform("u_far", camera.farClip);
            waterShader->uniform("u_lightPosition", float3(0.0, 10.0, 0.0));
            
            waterMesh.draw();
            waterShader->unbind();
            
            //glDisable(GL_BLEND);
        }
        
        draw_ui();
        
        gfx::gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
        
        frameCount++;
    }
    
};
