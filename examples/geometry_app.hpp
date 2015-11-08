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
    void draw() const { mesh.draw_elements(); };
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
    
    std::vector<ModelObject> proceduralModels;
    
    ExperimentalApp() : GLFWApp(640, 480, "Geometry App")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        grid = RenderableGrid(1, 100, 100);
        cameraController.set_camera(&camera);
        
        lights.resize(2);
        
        lights[0].color = float3(0.7f, 0.2f, 0.2f);
        lights[0].pose.position = float3(5, 10, -5);
        
        lights[1].color = float3(0.4f, 0.8f, 0.4f);
        lights[1].pose.position = float3(-5, 10, 5);
        
        simpleShader.reset(new gfx::GlShader(read_file_text("assets/shaders/simple_vert.glsl"), read_file_text("assets/shaders/simple_frag.glsl")));
        
        proceduralModels.resize(11);
        
        proceduralModels[0].mesh = make_sphere_mesh(1.0);
        proceduralModels[0].pose.position = float3(5, 0, 2);
        
        proceduralModels[1].mesh = make_cube_mesh();
        proceduralModels[1].pose.position = float3(5, 0, 4);
        
        proceduralModels[2].mesh = make_frustum_mesh();
        proceduralModels[2].pose.position = float3(5, 0, 6);
        
        proceduralModels[3].mesh = make_torus_mesh();
        proceduralModels[3].pose.position = float3(10, 4, -10);
        
        proceduralModels[4].mesh = make_capsule_mesh(8, 1, 3);
        proceduralModels[4].pose.position = float3(5, 0, 10);
        
        proceduralModels[5].mesh = make_plane_mesh(2, 2, 1, 1);
        proceduralModels[5].pose.position = float3(-5, 0, 2);
        
        proceduralModels[6].mesh = make_axis_mesh();
        proceduralModels[6].pose.position = float3(-5, 2, 4);
        
        proceduralModels[7].mesh = make_spiral_mesh();
        proceduralModels[7].pose.position = float3(-5, 0, 6);
        
        proceduralModels[8].mesh = make_icosahedron_mesh();
        proceduralModels[8].pose.position = float3(-10, 0, 8);
        
        proceduralModels[9].mesh = make_octohedron_mesh();
        proceduralModels[9].pose.position = float3(-15, 0, 10);
        
        proceduralModels[10].mesh = make_tetrahedron_mesh();
        proceduralModels[10].pose.position = float3(-20, 0, 12);
        
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

            for (const auto & model : proceduralModels)
            {
                simpleShader->uniform("u_modelMatrix", model.get_model());
                simpleShader->uniform("u_modelMatrixIT", inv(transpose(model.get_model())));
                model.draw();
            }
            
            simpleShader->unbind();
        }
        
        grid.render(proj, view);

        gfx::gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
        
        frameCount++;
    }
    
};
