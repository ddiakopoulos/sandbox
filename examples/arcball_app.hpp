#include "index.hpp"

using namespace math;
using namespace util;
using namespace gfx;

//object.scale *= 0.05f;
//object.rebuild_mesh();

//Pose p = Pose({0, 0, 0, 1}, -object.bounds.center());
//for (auto & v : object.geom.vertices)
//    v = transform_coord(p.matrix(), v);

struct ExperimentalApp : public GLFWApp
{
    Renderable object;
    
    GlMesh fullscreen_vignette_quad;
    
    GlTexture crateDiffuseTex;
    GlTexture crateNormalTex;
    GlTexture matcapTex;
    
    std::shared_ptr<GlShader> simpleTexturedShader;
    std::shared_ptr<GlShader> vignetteShader;
    std::shared_ptr<GlShader> matcapShader;
    
    ShaderMonitor shaderMonitor;
    
    GlCamera camera;
    Sphere cameraSphere;
    Arcball myArcball;
    
    float2 lastCursor;
    bool isDragging = false;
    bool useNormal = false;
    
    void rescale_geometry(Geometry & g, float radius = 1.0f)
    {
        auto bounds = g.compute_bounds();

        float3 r = 0.50f * (bounds.max - bounds.min);
        
        float3 center = bounds.min + r;
        
        float oldRadius = std::max(r.x, std::max(r.y, r.z));
        float scale = radius / oldRadius;
        
        for (auto & v : g.vertices)
        {
            float3 fixed = center + scale * (v - center);
            v = fixed;
        }
    }
    
    ExperimentalApp() : GLFWApp(1280, 720, "Arcball Camera App")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        object = Renderable(load_geometry_from_ply("assets/models/geometry/HexagonUniform.ply"));
        object.pose.position = {0, 0, 0};
        
        std::cout << "Object Scale: " << std::fixed << object.bounds.center() << std::endl;
        std::cout << "Object Volume: " << std::fixed << object.bounds.volume() << std::endl;
        std::cout << "Object Center: " << std::fixed << object.bounds.center() << std::endl;
        
        rescale_geometry(object.geom, 1.0f);
        object.rebuild_mesh();
        
        simpleTexturedShader.reset(new gfx::GlShader(read_file_text("assets/shaders/simple_texture_vert.glsl"), read_file_text("assets/shaders/simple_texture_frag.glsl")));
        shaderMonitor.add_shader(simpleTexturedShader, "assets/shaders/simple_texture_vert.glsl", "assets/shaders/simple_texture_frag.glsl");
        
        vignetteShader.reset(new gfx::GlShader(read_file_text("assets/shaders/vignette_vert.glsl"), read_file_text("assets/shaders/vignette_frag.glsl")));
        matcapShader.reset(new gfx::GlShader(read_file_text("assets/shaders/matcap_vert.glsl"), read_file_text("assets/shaders/matcap_frag.glsl")));
        
        crateDiffuseTex = load_image("assets/models/barrel/barrel_2_diffuse.png");
        crateNormalTex = load_image("assets/models/barrel/barrel_normal.png");
        matcapTex = load_image("assets/textures/matcap/metal_heated.png");
        
        fullscreen_vignette_quad = make_fullscreen_quad();
        
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

        if (event.type == InputEvent::CURSOR && event.drag)
        {
            myArcball.mouse_drag(event.cursor, event.windowSize);
        }
        
        if (event.type == InputEvent::MOUSE && event.is_mouse_down())
        {
            myArcball.mouse_down(event.cursor, event.windowSize);
        }
        
        lastCursor = event.cursor;
    }
    
    void on_update(const UpdateEvent & e) override
    {
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
        
        vignetteShader->bind();
        vignetteShader->uniform("u_noiseAmount", 0.1f);
        vignetteShader->uniform("u_screenResolution", float2(width, height));
        vignetteShader->uniform("u_backgroundColor", float3(20.f / 255.f, 20.f / 255.f, 20.f / 255.f));
        fullscreen_vignette_quad.draw_elements();
        vignetteShader->unbind();
        
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
        
        /*
        {
            matcapShader->bind();
            
            auto model = object.get_model();
            matcapShader->uniform("u_viewProj", viewProj);
            matcapShader->uniform("u_modelMatrix", model);
            matcapShader->uniform("u_modelViewMatrix", view * model);
            matcapShader->uniform("u_modelMatrixIT", get_rotation_submatrix(inv(transpose(model))));
            
            matcapShader->texture("u_matcapTexture", 0, matcapTex.get_gl_handle(), GL_TEXTURE_2D);
            
            object.draw();
            
            matcapShader->unbind();
        }
        */
        
        gfx::gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
    }
    
};