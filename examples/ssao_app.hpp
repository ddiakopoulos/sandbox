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
    math::Box<float, 3> bounds;
};

struct ModelObject : public Object
{
    GlMesh mesh;
    void draw() const { mesh.draw_elements(); };
};

struct ExperimentalApp : public GLFWApp
{
    
    ModelObject sofaModel;
    Geometry sofaGeometry;
    
    std::unique_ptr<GlShader> simpleShader;
    std::unique_ptr<GlShader> ssaoShader;
    
    std::unique_ptr<GLTextureView> colorTextureView;
    std::unique_ptr<GLTextureView> depthTextureView;
    std::unique_ptr<GLTextureView> outputTextureView;
    
    GlMesh fullscreen_post_quad;
    
    GlFramebuffer sceneFramebuffer;
    GlTexture sceneColorTexture;
    GlTexture sceneDepthTexture;
    
    GlTexture outputTexture;
    GlFramebuffer outputFbo;
    
    RenderableGrid grid;
    PreethamProceduralSky skydome;
    
    GlCamera camera;
    FPSCameraController cameraController;
    
    NVGcontext * nvgCtx;
    UWidget rootWidget;
    
    int enableAo = 0;
    
    ExperimentalApp() : GLFWApp(600, 600, "SSAO App")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        sofaGeometry = load_geometry_from_ply("assets/models/sofa/sofa.ply");
        sofaModel.mesh = make_mesh_from_geometry(sofaGeometry);
        sofaModel.bounds = sofaGeometry.compute_bounds();
        
        simpleShader.reset(new gfx::GlShader(read_file_text("assets/shaders/simple_vert.glsl"), read_file_text("assets/shaders/simple_frag.glsl")));
        
        cameraController.set_camera(&camera);
        camera.fov = 75;
        
        camera.look_at({0, 1.5, 3}, {0, 0, 0});
        
        grid = RenderableGrid(1, 100, 100);
        
        nvgCtx = make_nanovg_context(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
        if (!nvgCtx) throw std::runtime_error("error initializing nanovg context");

        gfx::gl_check_error(__FILE__, __LINE__);
        
        ssaoShader.reset(new gfx::GlShader(read_file_text("assets/shaders/post_vertex.glsl"), read_file_text("assets/shaders/arkano_ssao_frag.glsl")));
        
        fullscreen_post_quad = make_fullscreen_quad();
        
        sceneColorTexture.load_data(width, height, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        sceneDepthTexture.load_data(width, height, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        
        sceneFramebuffer.attach(GL_COLOR_ATTACHMENT0, sceneColorTexture);
        sceneFramebuffer.attach(GL_DEPTH_ATTACHMENT, sceneDepthTexture);
        if (!sceneFramebuffer.check_complete()) throw std::runtime_error("incomplete framebuffer");
        
        outputTexture.load_data(width, height, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        outputFbo.attach(GL_COLOR_ATTACHMENT0, outputTexture);
        if (!outputFbo.check_complete()) throw std::runtime_error("incomplete framebuffer");
        
        colorTextureView.reset(new GLTextureView(sceneColorTexture.get_gl_handle()));
        depthTextureView.reset(new GLTextureView(sceneDepthTexture.get_gl_handle()));
        outputTextureView.reset(new GLTextureView(outputTexture.get_gl_handle()));
        
        gfx::gl_check_error(__FILE__, __LINE__);
        
        // Set up the UI
        rootWidget.bounds = {0, 0, (float) width, (float) height};
        rootWidget.add_child( {{0,+10},{0,+10},{0.5,0},{0.5,0}}, std::make_shared<UWidget>()); // for colorTexture
        rootWidget.add_child( {{.50,+10},{0, +10},{1.0, -10},{0.5,0}}, std::make_shared<UWidget>()); // for depthTexture
        rootWidget.add_child( {{.0,+10},{0.5, +10},{0.5, 0},{1.0, -10}}, std::make_shared<UWidget>()); // for outputTexture
        rootWidget.layout();
    }
    
    void on_window_resize(math::int2 size) override
    {
        rootWidget.bounds = {0, 0, (float) size.x, (float) size.y};
        rootWidget.layout();
    }
    
    void on_input(const InputEvent & event) override
    {
        if (event.type == InputEvent::KEY)
        {
            if (event.value[0] == GLFW_KEY_1 && event.action == GLFW_RELEASE)
            {
                enableAo = 1;
            }
            else if (event.value[0] == GLFW_KEY_2 && event.action == GLFW_RELEASE)
            {
                enableAo = 0;
            }
        }
        cameraController.handle_input(event);
    }
    
    void on_update(const UpdateEvent & e) override
    {
        cameraController.update(e.timestep_ms);
    }
    
    void draw_ui()
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        nvgBeginFrame(nvgCtx, width, height, 1.0);
        {
            for (auto widget : rootWidget.children)
            {
                nvgBeginPath(nvgCtx);
                nvgRect(nvgCtx, widget->bounds.x0, widget->bounds.y0, widget->bounds.width(), widget->bounds.height());
                nvgStrokeColor(nvgCtx, nvgRGBA(255, 255, 255, 255));
                nvgStrokeWidth(nvgCtx, 1.0f);
                nvgStroke(nvgCtx);
            }
        }
        nvgEndFrame(nvgCtx);
        
        colorTextureView->draw(rootWidget.children[0]->bounds, int2(width, height));
        depthTextureView->draw(rootWidget.children[1]->bounds, int2(width, height));
        outputTextureView->draw(rootWidget.children[2]->bounds, int2(width, height));
    }
    
    void on_draw() override
    {
        glfwMakeContextCurrent(window);
        
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_POLYGON_OFFSET_FILL);
        
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        
        const auto proj = camera.get_projection_matrix((float) width / (float) height);
        const float4x4 view = camera.get_view_matrix();
        const float4x4 viewProj = mul(proj, view);
        
        // Draw into the framebuffer
        sceneFramebuffer.bind_to_draw();
        {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            
            skydome.render(viewProj, camera.get_eye_point(), camera.farClip);
            
            GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };
            glDrawBuffers(1, drawBuffers);
            
            gfx::gl_check_error(__FILE__, __LINE__);
            
            // Simple Shader
            {
                simpleShader->bind();
                
                simpleShader->uniform("u_viewProj", viewProj);
                simpleShader->uniform("u_eye", camera.get_eye_point());
                
                simpleShader->uniform("u_emissive", float3(.33f, 0.36f, 0.275f));
                simpleShader->uniform("u_diffuse", float3(0.2f, 0.4f, 0.25f));
                
                simpleShader->uniform("u_lights[0].position", float3(5, 10, -5));
                simpleShader->uniform("u_lights[0].color", float3(0.7f, 0.2f, 0.2f));
                
                simpleShader->uniform("u_lights[1].position", float3(-5, 10, 5));
                simpleShader->uniform("u_lights[1].color", float3(0.4f, 0.8f, 0.4f));
                
                {
                    sofaModel.pose.position = float3(0, 0, 0);
                    auto model = mul(sofaModel.pose.matrix(), make_scaling_matrix(0.001));
                    simpleShader->uniform("u_modelMatrix", model);
                    simpleShader->uniform("u_modelMatrixIT", inv(transpose(model)));
                    sofaModel.draw();
                }
                
                simpleShader->unbind();
            }
            grid.render(proj, view);
            gfx::gl_check_error(__FILE__, __LINE__);
        }
        
        // Draw into the output FBO
        {
            outputFbo.bind_to_draw();
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            
            // SSAO Postprocessing shader
            {
                ssaoShader->bind();
                
                ssaoShader->texture("u_colorTexture", 0, sceneColorTexture);
                ssaoShader->texture("u_depthTexture", 1, sceneDepthTexture);
                ssaoShader->uniform("u_useNoise", 1);
                ssaoShader->uniform("u_useMist", 0);
                ssaoShader->uniform("u_aoOnly", enableAo);
                ssaoShader->uniform("u_cameraNearClip", camera.nearClip);
                ssaoShader->uniform("u_cameraFarClip", camera.farClip);
                ssaoShader->uniform("u_resolution", float2(width, height));
                
                // Passthrough geometry
                fullscreen_post_quad.draw_elements();
                
                ssaoShader->unbind();
            }
            gfx::gl_check_error(__FILE__, __LINE__);
        }
        
        // Bind to 0
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        draw_ui();
        
        glfwSwapBuffers(window);
        
        gfx::gl_check_error(__FILE__, __LINE__);
    }
    
};
