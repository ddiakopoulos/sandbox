#include "index.hpp"

using namespace math;
using namespace util;
using namespace gfx;

struct ExperimentalApp : public GLFWApp
{
    Renderable object;
    
    GlTexture crateDiffuseTex;
    GlTexture crateNormalTex;
    
    std::shared_ptr<GlShader> simpleTexturedShader;
    std::shared_ptr<GlShader> vignetteShader;
    
    ShaderMonitor shaderMonitor;
    
    GlCamera camera;
    Sphere cameraSphere;
    Arcball myArcball;
    
    float2 lastCursor;
    bool isDragging = false;
    bool useNormal = false;
    
    ExperimentalApp() : GLFWApp(600, 600, "Arcball Camera App")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        object = Renderable(load_geometry_from_ply("assets/models/barrel/barrel.ply"));
        
        Pose p = Pose({0, 0, 0, 1}, -object.bounds.center());
        for (auto & v : object.geom.vertices)
            v = transform_coord(p.matrix(), v);
        
        object.rebuild_mesh();
        
        object.pose.position = {0, 0, 0};
        
        simpleTexturedShader.reset(new gfx::GlShader(read_file_text("assets/shaders/simple_texture_vert.glsl"), read_file_text("assets/shaders/simple_texture_frag.glsl")));
        shaderMonitor.add_shader(simpleTexturedShader, "assets/shaders/simple_texture_vert.glsl", "assets/shaders/simple_texture_frag.glsl");
        
        vignetteShader.reset(new gfx::GlShader(read_file_text("assets/shaders/vignette_vert.glsl"), read_file_text("assets/shaders/vignette_frag.glsl")));
        
        crateDiffuseTex = load_image("assets/models/barrel/barrel_2_diffuse.png");
        crateNormalTex = load_image("assets/models/barrel/barrel_normal.png");
        
        gfx::gl_check_error(__FILE__, __LINE__);
        
        cameraSphere = Sphere({0, 0, 0}, 6);
        myArcball = Arcball(&camera, cameraSphere);
        
        camera.look_at({0, 0, 10}, {0, 0, 0});
        //myArcball.set_constraint_axis(float3(0, 1, 0));
        
        gfx::gl_check_error(__FILE__, __LINE__);
    }
    
    void on_window_resize(math::int2 size) override
    {
        
    }
    
    void on_input(const InputEvent & event) override
    {
        
        if (event.type == InputEvent::KEY)
        {
            if (event.value[0] == GLFW_KEY_N && event.action == GLFW_RELEASE)
            {
                useNormal = !useNormal;
            }
            
        }
        if (event.type == InputEvent::CURSOR && isDragging)
        {
            if (event.cursor != lastCursor)
            {
                myArcball.mouse_drag(event.cursor, event.windowSize);
            }
        }
        
        if (event.type == InputEvent::MOUSE)
        {
            if (event.is_mouse_down())
            {
                isDragging = true;
                myArcball.mouse_down(event.cursor, event.windowSize);
            }
            
            if (event.is_mouse_up())
            {
                isDragging = false;
            }
        }
        
        lastCursor = event.cursor;
    }
    
    void on_update(const UpdateEvent & e) override
    {
        //if (isDragging)
            object.pose.orientation = qmul(myArcball.get_quat(), object.pose.orientation);
        
        shaderMonitor.handle_recompile();
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
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        
        const auto proj = camera.get_projection_matrix((float) width / (float) height);
        const float4x4 view = camera.get_view_matrix();
        const float4x4 viewProj = mul(proj, view);
        
        {
            simpleTexturedShader->bind();
            
            simpleTexturedShader->uniform("u_viewProj", viewProj);
            simpleTexturedShader->uniform("u_eye", camera.get_eye_point());
            
            simpleTexturedShader->uniform("u_emissive", float3(.5f, 0.5f, 0.5f));
            simpleTexturedShader->uniform("u_diffuse", float3(0.7f, 0.7f, 0.7f));
            
            simpleTexturedShader->uniform("u_lights[0].position", float3(6, 10, -6));
            simpleTexturedShader->uniform("u_lights[0].color", float3(0.7f, 0.2f, 0.2f));
            
            simpleTexturedShader->uniform("u_lights[1].position", float3(-6, 10, 6));
            simpleTexturedShader->uniform("u_lights[1].color", float3(0.4f, 0.8f, 0.4f));
            
            simpleTexturedShader->texture("u_diffuseTex", 0, crateDiffuseTex.get_gl_handle(), GL_TEXTURE_2D);
            simpleTexturedShader->texture("u_normalTex", 1, crateNormalTex.get_gl_handle(), GL_TEXTURE_2D);
            simpleTexturedShader->uniform("useNormal", useNormal);
            
            {
                auto model = object.get_model();
                simpleTexturedShader->uniform("u_modelMatrix", model);
                simpleTexturedShader->uniform("u_modelMatrixIT", inv(transpose(model)));
                object.draw();
            }
            
            simpleTexturedShader->unbind();
        }
        
        gfx::gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
    }
    
};