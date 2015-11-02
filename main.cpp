#include <iostream>
#include <sstream>

#include "util.hpp"
#include "string_utils.hpp"
#include "geometric.hpp"
#include "linear_algebra.hpp"
#include "math_util.hpp"
#include "circular_buffer.hpp"
#include "concurrent_queue.hpp"
#include "try_locker.hpp"
#include "running_statistics.hpp"
#include "time_keeper.hpp"
#include "human_time.hpp"
#include "signal.hpp" // todo: rename
#include "one_euro.hpp"
#include "json.hpp"
#include "geometry.hpp"
#include "pid_controller.hpp"
#include "base64.hpp"
#include "dsp_filters.hpp"
#include "bit_mask.hpp"
#include "file_io.hpp"
#include "GlMesh.hpp"
#include "GlShader.hpp"
#include "GlTexture.hpp"
#include "universal_widget.hpp"
#include "arcball.hpp"
#include "sketch.hpp"
#include "glfw_app.hpp"
#include "renderable_grid.hpp"
#include "procedural_sky.hpp"
#include "nvg.hpp"
#include "nanovg_gl.h"
#include "gpu_timer.hpp"

using namespace math;
using namespace util;
using namespace tinyply;
using namespace gfx;

static const float TEXT_OFFSET_X = 3;
static const float TEXT_OFFSET_Y = 1;

struct ExperimentalApp : public GLFWApp
{
    
    Model sofaModel;
    Geometry sofaGeometry;
    
    GlTexture crateDiffuseTex;
    
    std::unique_ptr<GLTextureView> colorTextureView;
    std::unique_ptr<GLTextureView> depthTextureView;
    std::unique_ptr<GLTextureView> outputTextureView;
    
    std::unique_ptr<GlShader> simpleShader;
    
    std::unique_ptr<GlShader> ssaoShader;
    std::unique_ptr<GlShader> filmGrainShader;
    std::unique_ptr<GlShader> fxaaShader;
    
    UWidget rootWidget;
    
    GlCamera camera;
    Sphere cameraSphere;
    //Arcball myArcball;
    
    float2 lastCursor;
    bool isDragging = false;
    
    RenderableGrid grid;
    
    FPSCameraController cameraController;
    PreethamProceduralSky skydome;
    
    NVGcontext * nvgCtx;
    std::shared_ptr<NvgFont> sourceFont;
    
    GlFramebuffer sceneFramebuffer;
    GlTexture sceneColorTexture;
    GlTexture sceneDepthTexture;
    
    //GlRenderbuffer sceneDepthBuffer;
    GlMesh sceneQuad;
    
    GlTexture outputTexture;
    GlFramebuffer outputFbo;
    
    int enableAo = 0;
    
    GPUTimer * sceneTimer = new GPUTimer("Scene");
    
    ExperimentalApp() : GLFWApp(600, 600, "Experimental App")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        sofaGeometry = load_geometry_from_ply("assets/models/crate/crate.ply");
        
        sofaModel.mesh = make_mesh_from_geometry(sofaGeometry);
        sofaModel.bounds = sofaGeometry.compute_bounds();
        
        gfx::gl_check_error(__FILE__, __LINE__);
        
        simpleShader.reset(new gfx::GlShader(read_file_text("assets/shaders/simple_texture_vert.glsl"), read_file_text("assets/shaders/simple_texture_frag.glsl")));
        
        //std::vector<uint8_t> whitePixel = {255,255,255,255};
        //emptyTex.load_data(1, 1, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel.data());
        
        crateDiffuseTex = load_image("assets/models/crate/crate_diffuse.png");
        
        rootWidget.bounds = {0, 0, (float) width, (float) height};
        rootWidget.add_child( {{0,+10},{0,+10},{0.5,0},{0.5,0}}, std::make_shared<UWidget>()); // for colorTexture
        rootWidget.add_child( {{.50,+10},{0, +10},{1.0, -10},{0.5,0}}, std::make_shared<UWidget>()); // for depthTexture
        rootWidget.add_child( {{.0,+10},{0.5, +10},{0.5, 0},{1.0, -10}}, std::make_shared<UWidget>()); // for outputTexture
        
        rootWidget.layout();
        
        cameraController.set_camera(&camera);
        camera.fov = 75;
        
        grid = RenderableGrid(1, 100, 100);
        
        nvgCtx = make_nanovg_context(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
        
        if (!nvgCtx) throw std::runtime_error("error initializing nanovg context");
        
        sourceFont = std::make_shared<NvgFont>(nvgCtx, "souce_sans_pro", read_file_binary("assets/fonts/source_code_pro_regular.ttf"));

        ssaoShader.reset(new gfx::GlShader(read_file_text("assets/shaders/post_vertex.glsl"), read_file_text("assets/shaders/arkano_ssao_frag.glsl")));
        
        filmGrainShader.reset(new gfx::GlShader(read_file_text("assets/shaders/post_vertex.glsl"), read_file_text("assets/shaders/filmgrain_frag.glsl")));
        fxaaShader.reset(new gfx::GlShader(read_file_text("assets/shaders/post_vertex.glsl"), read_file_text("assets/shaders/fxaa_frag.glsl")));
        
        gfx::gl_check_error(__FILE__, __LINE__);
        
        sceneColorTexture.load_data(width, height, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        sceneDepthTexture.load_data(width, height, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        
        sceneQuad = make_fullscreen_quad();
        //sceneDepthBuffer = GlRenderbuffer(GL_DEPTH_COMPONENT16, width, height);
        sceneFramebuffer.attach(GL_COLOR_ATTACHMENT0, sceneColorTexture);
        sceneFramebuffer.attach(GL_DEPTH_ATTACHMENT, sceneDepthTexture);
        if (!sceneFramebuffer.check_complete()) throw std::runtime_error("incomplete framebuffer");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
        outputTexture.load_data(width, height, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        outputFbo.attach(GL_COLOR_ATTACHMENT0, outputTexture);
        if (!outputFbo.check_complete()) throw std::runtime_error("incomplete framebuffer");
        
        colorTextureView.reset(new GLTextureView(sceneColorTexture.get_gl_handle()));
        depthTextureView.reset(new GLTextureView(sceneDepthTexture.get_gl_handle()));
        outputTextureView.reset(new GLTextureView(outputTexture.get_gl_handle()));

        gfx::gl_check_error(__FILE__, __LINE__);
        
        //cameraSphere = Sphere(sofaModel.bounds.center(), 1);
        //myArcball = Arcball(&camera, cameraSphere);
        //myArcball.set_constraint_axis(float3(0, 1, 0));
        
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

            if (event.value[0] == GLFW_KEY_EQUAL && event.action == GLFW_RELEASE)
            {
                camera.fov += 1;
                std::cout << camera.fov << std::endl;
            }
            if (event.value[0] == GLFW_KEY_1 && event.action == GLFW_RELEASE)
            {
                std::cout << "AO On" << std::endl;
                enableAo = 1;
            }
            if (event.value[0] == GLFW_KEY_2 && event.action == GLFW_RELEASE)
            {
                std::cout << "AO Off" << std::endl;
                enableAo = 0;
            }
        }
        if (event.type == InputEvent::CURSOR && isDragging)
        {
            //if (event.cursor != lastCursor)
               // myArcball.mouse_drag(event.cursor, event.windowSize);
        }
        
        if (event.type == InputEvent::MOUSE)
        {
            if (event.is_mouse_down())
            {
                isDragging = true;
                //myArcball.mouse_down(event.cursor, event.windowSize);
            }
            
            if (event.is_mouse_up())
            {
                isDragging = false;
                //myArcball.mouse_down(event.cursor, event.windowSize);
            }
        }
        
        cameraController.handle_input(event);
        lastCursor = event.cursor;
    }
    
    void on_update(const UpdateEvent & e) override
    {
        cameraController.update(e.elapsed_s / 1000);
    }
    
    void draw_ui()
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        nvgBeginFrame(nvgCtx, width, height, 1.0);
        {
            nvgBeginPath(nvgCtx);
            nvgRect(nvgCtx, 10, 10, 200, 150);
            nvgStrokeColor(nvgCtx, nvgRGBA(255, 255, 255, 127));
            nvgStrokeWidth(nvgCtx, 2.0f);
            nvgStroke(nvgCtx);
            
            for (auto widget : rootWidget.children)
            {
                nvgBeginPath(nvgCtx);
                nvgRect(nvgCtx, widget->bounds.x0, widget->bounds.y0, widget->bounds.width(), widget->bounds.height());
                nvgStrokeColor(nvgCtx, nvgRGBA(255, 255, 255, 255));
                nvgStrokeWidth(nvgCtx, 1.0f);
                nvgStroke(nvgCtx);
            }
            
            std::string text = "Hello NanoVG";
            const float textX = 15 + TEXT_OFFSET_X, textY = 15 + TEXT_OFFSET_Y;
            nvgFontFaceId(nvgCtx, sourceFont->id);
            nvgFontSize(nvgCtx, 20);
            nvgTextAlign(nvgCtx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
            nvgBeginPath(nvgCtx);
            nvgFillColor(nvgCtx, nvgRGBA(0,0,0,255));
            auto ret = nvgText(nvgCtx, textX, textY, text.c_str(), nullptr);
            
        }
        nvgEndFrame(nvgCtx);
    }
    
    void on_draw() override
    {
        //sceneTimer->start();
        
        static int frameCount = 0;
        
        glfwMakeContextCurrent(window);
        
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        
        //glDepthFunc(GL_LEQUAL);
        //glEnable(GL_BLEND);
        //glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_POLYGON_OFFSET_FILL);
        
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
     
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.1f, 0.1f, 0.5f, 1.0f);

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
            
            {
                simpleShader->bind();
                
                simpleShader->uniform("u_viewProj", viewProj);
                simpleShader->uniform("u_eye", float3(0, 10, -10));
                
                simpleShader->uniform("u_emissive", float3(.33f, 0.36f, 0.275f));
                simpleShader->uniform("u_diffuse", float3(0.2f, 0.4f, 0.25f));
                
                simpleShader->uniform("u_lights[0].position", float3(5, 10, -5));
                simpleShader->uniform("u_lights[0].color", float3(0.7f, 0.2f, 0.2f));
                
                simpleShader->uniform("u_lights[1].position", float3(-5, 10, 5));
                simpleShader->uniform("u_lights[1].color", float3(0.4f, 0.8f, 0.4f));
                
                simpleShader->texture("u_diffuseTex", 0, crateDiffuseTex.get_gl_handle(), GL_TEXTURE_2D);
                
                {
                    sofaModel.pose.position = float3(0, 0, 0);
                    auto model = mul(sofaModel.pose.matrix(), make_scaling_matrix(0.1));
                    simpleShader->uniform("u_modelMatrix", model);
                    simpleShader->uniform("u_modelMatrixIT", inv(transpose(model)));
                    sofaModel.draw();
                }
                
                simpleShader->unbind();
            }
            grid.render(proj, view);
            gfx::gl_check_error(__FILE__, __LINE__);
        }

        outputFbo.bind_to_draw();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        
        gfx::gl_check_error(__FILE__, __LINE__);
        
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
            sceneQuad.draw_elements();

            ssaoShader->unbind();
        }
        
        glBindTexture(GL_TEXTURE_2D, 0);
        
        // Bind to 0
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
        colorTextureView->draw(rootWidget.children[0]->bounds, int2(width, height));
        depthTextureView->draw(rootWidget.children[1]->bounds, int2(width, height));
        outputTextureView->draw(rootWidget.children[2]->bounds, int2(width, height));
        
        gfx::gl_check_error(__FILE__, __LINE__);
        
        draw_ui();
        
        glfwSwapBuffers(window);
        
        frameCount++;
        
        //sceneTimer->stop();
    }
    
};

//#include "examples/sandbox_app.hpp"
//#include "examples/ssao_app.hpp"
//#include "examples/arcball_app.hpp"
//#include "examples/skydome_app.hpp"

IMPLEMENT_MAIN(int argc, char * argv[])
{
    ExperimentalApp app;
    app.main_loop();
    return 0;
}
