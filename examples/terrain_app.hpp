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
        //terrainShader->uniform("u_clipPlane", float4(0.0, 0.0, 1.0, -yWaterPlane)); // water
        terrainShader->uniform("u_lightPosition", float3(0.0, 0.0, -5.0));
        terrainShader->texture("u_noiseTexture", 0, perlinTexture.get_gl_handle(), GL_TEXTURE_2D);
        
        terrainMesh.draw();
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
    
    float4 euler_to_quat(float roll, float pitch, float yaw)
    {
        double sy = sin(yaw * 0.5);
        double cy = cos(yaw * 0.5);
        double sp = sin(pitch * 0.5);
        double cp = cos(pitch * 0.5);
        double sr = sin(roll * 0.5);
        double cr = cos(roll * 0.5);
        double w = cr*cp*cy + sr*sp*sy;
        double x = sr*cp*cy - cr*sp*sy;
        double y = cr*sp*cy + sr*cp*sy;
        double z = cr*cp*sy - sr*sp*cy;
        return float4(x,y,z,w);
    }
    
    float3 quat_to_euler(float4 q)
    {
        float3 e;
        const double q0 = q.w;
        const double q1 = q.x;
        const double q2 = q.y;
        const double q3 = q.z;
        e.x = atan2(2*(q0*q1+q2*q3), 1-2*(q1*q1+q2*q2));
        e.y = asin(2*(q0*q2-q3*q1));
        e.z = atan2(2*(q0*q3+q1*q2), 1-2*(q2*q2+q3*q3));
        return e;
    }
    
    void calculate_reflection_matrix (float4x4 & reflectionMat, float4 plane)
    {
        reflectionMat(0,0) = (1.f- 2.0f * plane[0]*plane[0]);
        reflectionMat(0,1) = (   - 2.0f * plane[0]*plane[1]);
        reflectionMat(0,2) = (   - 2.0f * plane[0]*plane[2]);
        reflectionMat(0,3) = (   - 2.0f * plane[3]*plane[0]);
        
        reflectionMat(1,0) = (   - 2.0f * plane[1]*plane[0]);
        reflectionMat(1,1) = (1.f- 2.0f * plane[1]*plane[1]);
        reflectionMat(1,2) = (   - 2.0f * plane[1]*plane[2]);
        reflectionMat(1,3) = (   - 2.0f * plane[3]*plane[1]);
        
        reflectionMat(2,0) = (   - 2.0f * plane[2]*plane[0]);
        reflectionMat(2,1) = (   - 2.0f * plane[2]*plane[1]);
        reflectionMat(2,2) = (1.f- 2.0f * plane[2]*plane[2]);
        reflectionMat(2,3) = (   - 2.0f * plane[3]*plane[2]);
        
        reflectionMat(3,0) = 0.0f;
        reflectionMat(3,1) = 0.0f;
        reflectionMat(3,2) = 0.0f;
        reflectionMat(3,3) = 1.0f;
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
            
            //glFrontFace(GL_CW);
            //glCullFace(GL_BACK);
            glDisable(GL_CULL_FACE);
            glEnable(GL_CLIP_PLANE0);
            
            reflectionFramebuffer.bind_to_draw();
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            
            reflectionCamera.set_position(camera.pose.position);
            reflectionCamera.set_orientation(camera.pose.orientation);
            
            // Reflect camera around reflection plane
            float3 normal = float3(0, 1, 0);
            float3 pos = {0, 0, 0}; //Location of object... here, the "terrain"
            float d = -dot(normal, pos) - yWaterPlane;
            float4 reflectionPlane = float4(normal.x, normal.y, normal.z, d);
            
            float4x4 reflection = Zero4x4;
            calculate_reflection_matrix(reflection, reflectionPlane);
            
            float4x4 flip = Zero4x4;
            calculate_reflection_matrix(flip, float4(0, 0, 1, 0));
            
            float3 oldPosition = camera.pose.position;
            float3 newPosition = transform_coord(reflection, oldPosition);
            
            reflectionCamera.set_position(newPosition);
            
            auto e = quat_to_euler(camera.pose.orientation);
            reflectionCamera.set_orientation(euler_to_quat(-e.x, e.y, e.z));
            
            float4x4 reflectedView = reflectionCamera.get_view_matrix() * reflection;
            
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
            
            terrainShader->unbind();
            
            glDisable(GL_CLIP_PLANE0);
            //glFrontFace(GL_CCW);
            
            gfx::gl_check_error(__FILE__, __LINE__);
            
            glDisable(GL_CLIP_PLANE0);
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

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        
        draw_ui();
        
        gfx::gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
        
        frameCount++;
    }
    
};
