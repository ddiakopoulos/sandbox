#include "anvil.hpp"

using namespace math;
using namespace util;
using namespace gfx;

struct Object
{
    Pose pose;
    float3 scale;
    Object() : scale(1, 1, 1) {}
    float4x4 get_model() const { return mul(pose.matrix(), make_scaling_matrix(scale)); }
};

struct ModelObject : public Object
{
    GlMesh mesh;
};

struct LightObject : public Object
{
    float3 color;
};

struct TexturedObject : public ModelObject
{
    GlTexture diffuseTexture;
    GlTexture normalTexture;
};

struct ExperimentalApp : public GLFWApp
{
    uint64_t frameCount = 0;

    GlCamera camera;
    PreethamProceduralSky preethamSky;
    RenderableGrid grid;
    FPSCameraController cameraController;
    
    std::unique_ptr<GlShader> simpleShader;
    
    std::vector<ModelObject> models;
    std::vector<LightObject> lights;
    std::vector<TexturedObject> texturedModels;
    
    ExperimentalApp() : GLFWApp(640, 480, "Geometry App")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        grid = RenderableGrid(1, 100, 100);
        cameraController.set_camera(&camera);
        gfx::gl_check_error(__FILE__, __LINE__);
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
        cameraController.update(e.timestep_ms);
    }
    
    void on_draw() override
    {
        glfwMakeContextCurrent(window);
        
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);

        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
     
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.1f, 0.1f, 0.5f, 1.0f);

        const auto proj = camera.get_projection_matrix((float) width / (float) height);
        const float4x4 view = camera.get_view_matrix();
        const float4x4 viewProj = mul(proj, view);
        
        preethamSky.render(viewProj, camera.get_eye_point(), camera.farClip);
        
        // Simple Shader
        {
            simpleShader->bind();
            
            simpleShader->uniform("u_viewProj", viewProj);
            simpleShader->uniform("u_eye", camera.get_eye_point());
            
            simpleShader->uniform("u_emissive", float3(.33f, 0.36f, 0.275f));
            simpleShader->uniform("u_diffuse", float3(0.2f, 0.4f, 0.25f));
            
            for (int i = 0; i < lights.size(); i++)
            {
                auto light = lights[i];
                
                simpleShader->uniform("u_lights[" + std::to_string(i) + "].position", light.pose.position);
                simpleShader->uniform("u_lights[" + std::to_string(i) + "].color", light.color);
            }
            /*
            {
                sofaModel.pose.position = float3(0, 0, 0);
                auto model = mul(sofaModel.pose.matrix(), make_scaling_matrix(0.001));
                simpleShader->uniform("u_modelMatrix", model);
                simpleShader->uniform("u_modelMatrixIT", inv(transpose(model)));
                sofaModel.draw();
            }
            */
            
            simpleShader->unbind();
        }
        
        grid.render(proj, view);

        gfx::gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
        
        frameCount++;
    }
    
};
