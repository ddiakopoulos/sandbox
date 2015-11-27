#include "anvil.hpp"
#include "noise.h"
#include <random>

using namespace math;
using namespace util;
using namespace gfx;

void find_orthonormals(const float3 normal, float3 & orthonormal1, float3 & orthonormal2)
{
    const float4x4 OrthoX = make_rotation_matrix({1, 0, 0}, ANVIL_PI / 2);
    const float4x4 OrthoY = make_rotation_matrix({0, 1, 0}, ANVIL_PI / 2);;

    float3 w = transform_vector(OrthoX, normal);
    float d = dot(normal, w);
    if (abs(d) > 0.6f)
    {
        w = transform_vector(OrthoY, normal);
    }
    
    w = normalize(w);
    
    orthonormal1 = cross(normal, w);
    orthonormal1 = normalize(orthonormal1);
    orthonormal2 = cross(normal, orthonormal1);
    orthonormal2 = normalize(orthonormal2);
}

float find_quaternion_twist(float4 q, float3 axis)
{
    normalize(axis);
    
    //get the plane the axis is a normal of
    float3 orthonormal1, orthonormal2;
    
    find_orthonormals(axis, orthonormal1, orthonormal2);
    
    float3 transformed = transform_vector(q, orthonormal1); // orthonormal1 * q;
    
    //project transformed vector onto plane
    float3 flattened = transformed - (dot(transformed, axis) * axis);
    flattened = normalize(flattened);
    
    //get angle between original vector and projected transform to get angle around normal
    float a = (float) acos(dot(orthonormal1, flattened));
    
    return a;
}

struct ExperimentalApp : public GLFWApp
{
    uint64_t frameCount = 0;

    GlCamera camera;
    GlCamera reflectionCamera;
    
    HosekProceduralSky skydome;
    FPSCameraController cameraController;
    
    GlTexture perlinTexture;
    
    std::vector<Renderable> proceduralModels;
    std::vector<LightObject> lights;

    std::unique_ptr<GlShader> terrainShader;
    std::unique_ptr<GlShader> waterShader;
    
    GlFramebuffer reflectionFramebuffer;
    GlTexture sceneColorTexture;
    
    GlFramebuffer depthFramebuffer;
    GlTexture sceneDepthTexture;
    
    Renderable terrainMesh, waterMesh;
    
    Renderable cubeMesh;
    
    std::unique_ptr<GLTextureView> colorTextureView;
    std::unique_ptr<GLTextureView> depthTextureView;
    
    std::mt19937 mt_rand;
    
    float appTime = 0;
    float yWaterPlane = 2.0f;
    
    UWidget rootWidget;
    
    ExperimentalApp() : GLFWApp(940, 720, "Sandbox App")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        cameraController.set_camera(&camera);
        
        //camera.look_at({0, 8, 24}, {0, 0, 0});

        perlinTexture = make_perlin_texture(16, 16);

        terrainShader.reset(new gfx::GlShader(read_file_text("assets/shaders/terrain_vert.glsl"), read_file_text("assets/shaders/terrain_frag.glsl")));
        waterShader.reset(new gfx::GlShader(read_file_text("assets/shaders/water_vert.glsl"), read_file_text("assets/shaders/water_frag.glsl")));
        
        sceneColorTexture.load_data(width, height, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        reflectionFramebuffer.attach(GL_COLOR_ATTACHMENT0, sceneColorTexture);
        if (!reflectionFramebuffer.check_complete()) throw std::runtime_error("incomplete framebuffer");
        
        sceneDepthTexture.load_data(width, height, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        depthFramebuffer.attach(GL_DEPTH_ATTACHMENT, sceneDepthTexture);
        if (!depthFramebuffer.check_complete()) throw std::runtime_error("incomplete framebuffer");
        
        gfx::gl_check_error(__FILE__, __LINE__);
        
        terrainMesh = Renderable(make_plane(96.f, 96.f, 128, 128));
        waterMesh = Renderable(make_plane(96.f, 96.f, 128, 128));
        
        cubeMesh = Renderable(make_cube());
        
        auto seedGenerator = std::bind(std::uniform_int_distribution<>(0, 512), std::ref(mt_rand));
        auto newSeed = seedGenerator();
        seed(newSeed);
        
        {
            lights.resize(2);
            lights[0].color = float3(249.f / 255.f, 228.f / 255.f, 157.f / 255.f);
            lights[0].pose.position = float3(25, 15, 0);
            lights[1].color = float3(255.f / 255.f, 242.f / 255.f, 254.f / 255.f);
            lights[1].pose.position = float3(-25, 15, 0);
        }
        
        colorTextureView.reset(new GLTextureView(sceneColorTexture.get_gl_handle()));
        depthTextureView.reset(new GLTextureView(sceneDepthTexture.get_gl_handle()));
        
        gfx::gl_check_error(__FILE__, __LINE__);
        
        // Set up the UI
        rootWidget.bounds = {0, 0, (float) width, (float) height};
        rootWidget.add_child( {{0,+10},{0,+10},{0.25,0},{0.25,0}}, std::make_shared<UWidget>()); // for colorTexture
        rootWidget.add_child( {{.25,+10},{0, +10},{0.50, -10},{0.25,0}}, std::make_shared<UWidget>()); // for depthTexture
        rootWidget.layout();
        
    }
    
    GlTexture make_perlin_texture(int width, int height)
    {
        GlTexture tex;
        
        std::vector<uint8_t> perlinNoise(width * height);
        
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                float h = 0.0f;
                float f = 0.05f;
                h += simplex2(x * f, y * f, 4.0f, 1.f, 6.0f) * 1; f /= 2.0f;
                h += simplex2(x * f, y * f, 4.0f, 2.f, 6.0f) * 2; f /= 2.0f;
                h += simplex2(x * f, y * f, 4.0f, 3.f, 4.0f) * 4; f /= 2.0f;
                h += simplex2(x * f, y * f, 4.0f, 4.f, 2.0f) * 8; f /= 2.0f;
                uint8_t noiseValue = remap<float>(h, 3.0f, 8.5f, 0.0f, 255.f, true);
                perlinNoise[y * height + x] = noiseValue;
            }
        }
        tex.load_data(width, height, GL_RED, GL_UNSIGNED_BYTE, perlinNoise.data());
        
        return tex;
    }
    
    void on_window_resize(math::int2 size) override
    {

    }
    
    void on_input(const InputEvent & event) override
    {
        cameraController.handle_input(event);
    }
    
    void on_update(const UpdateEvent & e) override
    {
        appTime = e.elapsed_s;
        cameraController.update(e.timestep_ms);
    }
    
    void draw_terrain()
    {
        glEnable(GL_BLEND);
        
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        
        float4x4 model = make_rotation_matrix({1, 0, 0}, ANVIL_PI / 2);
        float4x4 mvp = camera.get_projection_matrix((float) width / (float) height) * camera.get_view_matrix() * model;
        
        terrainShader->bind();
        terrainShader->uniform("u_mvp", mvp);
        terrainShader->uniform("u_modelMatrix",  model);
        terrainShader->uniform("u_modelMatrixIT", get_rotation_submatrix(inv(transpose(mvp))));
        terrainShader->uniform("u_clipPlane", float4(0.0, 0.0, 1.0, -yWaterPlane)); // water
        terrainShader->uniform("u_lightPosition", float3(0.0, 0.0, -5.0));
        terrainShader->texture("u_noiseTexture", 0, perlinTexture.get_gl_handle(), GL_TEXTURE_2D);
        
        terrainMesh.draw();
        cubeMesh.draw();
        
        terrainShader->unbind();
        
        glDisable(GL_BLEND);
        gfx::gl_check_error(__FILE__, __LINE__);
    }
    
    void draw_ui()
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        colorTextureView->draw(rootWidget.children[0]->bounds, int2(width, height));
        depthTextureView->draw(rootWidget.children[1]->bounds, int2(width, height));
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
        
        // This otherwise skybox doesn't render
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.f, 0.0f, 0.0f, 1.0f);
        
        float4x4 viewProj = camera.get_projection_matrix((float) width / (float) height) * camera.get_view_matrix();
        
        skydome.render(viewProj, camera.get_eye_point(), camera.farClip);
        
        {
            glFrontFace(GL_CW);
            
            glEnable(GL_CLIP_PLANE0);
            
            reflectionFramebuffer.bind_to_draw();
            glClear(GL_COLOR_BUFFER_BIT);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            
            float3 camPosition = camera.get_eye_point();
            camPosition.y = -camPosition.y * yWaterPlane * 2.f;
            
            reflectionCamera.set_position(camPosition);
            reflectionCamera.set_orientation(camera.pose.orientation); // same orientation as regular camera...
            //reflectionCamera.pose.orientation = qmul(make_rotation_quat_axis_angle({-1, 0, 0}, ANVIL_PI), reflectionCamera.pose.orientation);
            
             /*
              | 1-2Nx2   -2NxNy  -2NxNz  -2NxD |
Mreflection = |  -2NxNy 1-2Ny2   -2NyNz  -2NyD |
              |  -2NxNz  -2NyNz 1-2Nz2   -2NzD |
              |    0       0       0       1   |

             Where (Nx,Ny,Nz,D) are the coefficients of plane equation (xNx + yNy + zNz + D = 0). Notice, that (Nx,Ny,Nz) is also the normal vector of given plane.
             */
            
            float4x4 reflect = {{1, 0, 0, 0}, {0, -1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
            float3 n = {0, 1, 0};
    
            float4x4 reflectedView = reflect * reflectionCamera.get_view_matrix();
            float4x4 model = make_rotation_matrix({1, 0, 0}, ANVIL_PI / 2);
            float4x4 mvp = reflectionCamera.get_projection_matrix((float) width / (float) height) * reflectedView * model;
            
            terrainShader->bind();
            terrainShader->uniform("u_mvp", mvp);
            terrainShader->uniform("u_modelMatrix", model);
            terrainShader->uniform("u_modelMatrixIT", get_rotation_submatrix(inv(transpose(mvp))));
            terrainShader->uniform("u_clipPlane", float4(0.0, 0.0, 1.0, -yWaterPlane)); // water - http://trederia.blogspot.com/2014/09/water-in-opengl-and-gles-20-part3.html
            terrainShader->uniform("u_lightPosition", float3(0.0, 0.0, -5.0));
            terrainShader->texture("u_noiseTexture", 0, perlinTexture.get_gl_handle(), GL_TEXTURE_2D);
            
            terrainMesh.draw();
            cubeMesh.draw();
            
            terrainShader->unbind();
            
            glDisable(GL_CLIP_PLANE0);
            glFrontFace(GL_CCW);
            glEnable(GL_CULL_FACE);
            glEnable(GL_DEPTH_TEST);
            
            gfx::gl_check_error(__FILE__, __LINE__);
            reflectionFramebuffer.unbind();
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
            //glEnable(GL_BLEND);
            //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            float4x4 model = make_rotation_matrix({1, 0, 0}, ANVIL_PI / 2);
            auto mvp = camera.get_projection_matrix((float) width / (float) height) * camera.get_view_matrix() * model;
            
            waterShader->bind();
            
            waterShader->uniform("u_mvp", mvp);
            waterShader->uniform("u_time", appTime);
            waterShader->uniform("u_yWaterPlane", yWaterPlane);
            waterShader->uniform("u_modelMatrix", model);
            waterShader->uniform("u_modelMatrixIT", get_rotation_submatrix(inv(transpose(mvp))));
            waterShader->uniform("u_resolution", float2(width, height));
            
            waterShader->texture("u_reflectionTexture", 0, sceneColorTexture.get_gl_handle(), GL_TEXTURE_2D);
            waterShader->texture("u_depthTexture", 1, sceneDepthTexture.get_gl_handle(), GL_TEXTURE_2D);
            
            waterShader->uniform("u_near", camera.nearClip);
            waterShader->uniform("u_far", camera.farClip);
            waterShader->uniform("u_lightPosition", float3(0.0, 0.0, -5.0));
            
            waterMesh.draw();
            waterShader->unbind();
            
            //glDisable(GL_BLEND);
        }

        //glDisable(GL_DEPTH_TEST);
        //glDisable(GL_CULL_FACE);
        //glDisable(GL_BLEND);
        
        draw_ui();
        
        gfx::gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
        
        frameCount++;
    }
    
};
