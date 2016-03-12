#include "index.hpp"

// http://developer.download.nvidia.com/presentations/2008/GDC/GDC08_SoftShadowMapping.pdf
// https://mynameismjp.wordpress.com/2015/02/18/shadow-sample-update/
// https://blogs.aerys.in/jeanmarc-leroux/2015/01/21/exponential-cascaded-shadow-mapping-with-webgl/

// [ ] Simple Shadow Mapping (SSM)
// [ ] Exponential Shadow Mapping (ESM)
// [ ] Moment Shadow Mapping [MSM]
// [ ] Percentage Closer Filtering (PCF) + poisson disk sampling (PCSS + PCF)
// [ ] Shadow Volumes (face / edge)
// [ ] Variance Shadow Mapping (VSM) http://www.punkuser.net/vsm/vsm_paper.pdf

inline float mix(float a, float b, float t)
{
    return a * (1 - t) + b * t;
}

std::shared_ptr<GlShader> make_watched_shader(ShaderMonitor & mon, const std::string vertexPath, const std::string fragPath, const std::string geomPath = "")
{
    std::shared_ptr<GlShader> shader = std::make_shared<GlShader>(read_file_text(vertexPath), read_file_text(fragPath), read_file_text(geomPath));
    mon.add_shader(shader, vertexPath, fragPath);
    return shader;
}

struct DirectionalLight 
{
    float3 color;
    float3 direction;
    float size;
    
    DirectionalLight(const float3 dir, const float3 color, float size) : direction(dir), color(color), size(size) {}
    
    float4x4 get_view_proj_matrix(float3 eyePoint)
    {
        auto p = look_at_pose(eyePoint, eyePoint + -direction);
        const float halfSize = size * 0.5f;
        return mul(make_orthographic_matrix(-halfSize, halfSize, -halfSize, halfSize, -halfSize, halfSize), make_view_matrix_from_pose(p));
    }
};

struct SpotLight
{
    float3 color;
    float3 direction;
    
    float3 position;
    float cutoff;
    float3 attenuation; // constant, linear, quadratic
    
    SpotLight(const float3 pos, const float3 dir, const float3 color, float cut, float3 att) : position(pos), direction(dir), color(color), cutoff(cut), attenuation(att) {}
    
    float4x4 get_view_proj_matrix()
    {
        auto p = look_at_pose(position, position + -direction);
        return mul(make_perspective_matrix(to_radians(cutoff * 2.0f), 1.0f, 0.1f, 1000.f), make_view_matrix_from_pose(p));
    }
    
    float get_cutoff() { return cosf(to_radians(cutoff)); }
};

struct ExperimentalApp : public GLFWApp
{
    std::random_device rd;
    std::mt19937 gen;
    
    GlCamera camera;
    PreethamProceduralSky skydome;
    FlyCameraController cameraController;
    ShaderMonitor shaderMonitor;
    Space uiSurface;
    
    std::unique_ptr<gui::ImGuiManager> igm;
    
    std::shared_ptr<GLTextureView> viewA;
    std::shared_ptr<GLTextureView> viewB;
    std::shared_ptr<GLTextureView> viewC;
    std::shared_ptr<GLTextureView> viewD;
    
    std::shared_ptr<GlShader> sceneShader;
    std::shared_ptr<GlShader> shadowmapShader;
    std::shared_ptr<GlShader> gaussianBlurShader;
    
    std::vector<Renderable> sceneObjects;
    
    GlMesh fullscreen_post_quad;
    
    Renderable floor;
    Renderable lightFrustum;
    
    GlTexture shadowDepthTexture;
    GlFramebuffer shadowFramebuffer;
    
    GlTexture shadowBlurTexture;
    GlFramebuffer shadowBlurFramebuffer;
    
    std::shared_ptr<DirectionalLight> sunLight;
    std::vector<std::shared_ptr<SpotLight>> spotLights;
    
    const float shadowmapResolution = 2048;
    float blurSigma = 3.0f;
    
    ExperimentalApp() : GLFWApp(1280, 720, "Shadow App")
    {
        glfwSwapInterval(0);
        
        gen = std::mt19937(rd());
        
        igm.reset(new gui::ImGuiManager(window));
        gui::make_dark_theme();
        
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        cameraController.set_camera(&camera);
        camera.farClip = 55.f;
        camera.look_at({0, 0, +15}, {0, 0, 0});
        
        // Debugging views
        uiSurface.bounds = {0, 0, (float) width, (float) height};
        uiSurface.add_child( {{0.0000f, +10},{0, +10},{0.1667f, -10},{0.133f, +10}});
        uiSurface.add_child( {{0.1667f, +10},{0, +10},{0.3334f, -10},{0.133f, +10}});
        uiSurface.add_child( {{0.3334f, +10},{0, +10},{0.5009f, -10},{0.133f, +10}});
        uiSurface.add_child( {{0.5000f, +10},{0, +10},{0.6668f, -10},{0.133f, +10}});
        uiSurface.add_child( {{0.6668f, +10},{0, +10},{0.8335f, -10},{0.133f, +10}});
        uiSurface.add_child( {{0.8335f, +10},{0, +10},{1.0000f, -10},{0.133f, +10}});
        uiSurface.layout();
        
        fullscreen_post_quad = make_fullscreen_quad();
        
        sceneShader = make_watched_shader(shaderMonitor, "assets/shaders/shadow/scene_vert.glsl", "assets/shaders/shadow/scene_frag.glsl");
        shadowmapShader = make_watched_shader(shaderMonitor, "assets/shaders/shadow/shadowmap_vert.glsl", "assets/shaders/shadow/shadowmap_frag.glsl");
        gaussianBlurShader = make_watched_shader(shaderMonitor, "assets/shaders/gaussian_blur_vert.glsl", "assets/shaders/gaussian_blur_frag.glsl");
        
        skydome.recompute(2, 10.f, 1.15f);
        
        auto lightDir = skydome.get_light_direction();
        sunLight = std::make_shared<DirectionalLight>(lightDir, float3(.50f, .75f, .825f), 64.f);
        
        auto spotLightA = std::make_shared<SpotLight>(float3(0.f, 10.f, 0.f), float3(0.f, -1.f, 0.f), float3(1.f, 0.f, 0.f), 30.0f, float3(1.0f, 0.0f, 0.0001f));
        spotLights.push_back(spotLightA);
        
        // todo spotLightB
        
        shadowDepthTexture.load_data(shadowmapResolution, shadowmapResolution, GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        shadowFramebuffer.attach(GL_DEPTH_ATTACHMENT, shadowDepthTexture);
        if (!shadowFramebuffer.check_complete()) throw std::runtime_error("incomplete shadow framebuffer");
        
        shadowBlurTexture.load_data(shadowmapResolution, shadowmapResolution, GL_R32F, GL_RGBA, GL_FLOAT, nullptr);
        shadowBlurFramebuffer.attach(GL_COLOR_ATTACHMENT0, shadowBlurTexture);
        //shadowBlurFramebuffer.attach(GL_COLOR_ATTACHMENT1, shadowDepthTexture); // note this attach point
        if (!shadowBlurFramebuffer.check_complete()) throw std::runtime_error("incomplete blur framebuffer");
        
        viewA.reset(new GLTextureView(shadowDepthTexture.get_gl_handle()));
        viewB.reset(new GLTextureView(shadowBlurTexture.get_gl_handle()));
        //viewC.reset(new GLTextureView(get_gl_handle()));
        //viewD.reset(new GLTextureView(get_gl_handle()));
        
        auto leePerryHeadModel = load_geometry_from_obj_no_texture("assets/models/leeperrysmith/lps.obj");
        Geometry combined;
        for (int i = 0; i < leePerryHeadModel.size(); ++i)
        {
            auto & m = leePerryHeadModel[i];
            for (auto & v : m.vertices) v *= 25.f;
            combined = concatenate_geometry(combined, m);
        }
        combined.compute_normals(false);
        sceneObjects.push_back(Renderable(combined));
        
        auto lucy = load_geometry_from_ply("assets/models/stanford/lucy.ply");
        for (auto & vert : lucy.vertices)
        {
            vert *= .01f;
        }
        sceneObjects.push_back(Renderable(lucy));
		
        floor = Renderable(make_plane(24.f, 24.f, 256, 256), false);
        floor.pose.orientation = make_rotation_quat_axis_angle({1, 0, 0}, -ANVIL_PI / 2);
        floor.pose.position = {0, -7, 0};
        sceneObjects.push_back(std::move(floor));
        
        gl_check_error(__FILE__, __LINE__);
    }
    
    void on_window_resize(int2 size) override
    {
        
    }
    
    void on_input(const InputEvent & e) override
    {
        if (igm) igm->update_input(e);
        cameraController.handle_input(e);
    }
    
    void on_update(const UpdateEvent & e) override
    {
        cameraController.update(e.timestep_ms);
        shaderMonitor.handle_recompile();
    }
    
    void on_draw() override
    {
        
        glfwMakeContextCurrent(window);
        
        if (igm) igm->begin_frame();
        
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glDepthMask(GL_TRUE);
        
        float windowAspectRatio = (float) width / (float) height;
        
        const auto proj = camera.get_projection_matrix(windowAspectRatio);
        const float4x4 view = camera.get_view_matrix();
        const float4x4 viewProj = mul(proj, view);
        
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        skydome.render(viewProj, camera.get_eye_point(), camera.farClip);
    
        float3 target = camera.pose.position;
        
        // Render the scene from the perspective of the light source
        {
            shadowFramebuffer.bind_to_draw();
            shadowmapShader->bind();
            
            glClear(GL_DEPTH_BUFFER_BIT);
            glViewport(0, 0, shadowmapResolution, shadowmapResolution);
            
            shadowmapShader->uniform("u_lightViewProj", sunLight->get_view_proj_matrix(target));
            
            //glEnable(GL_POLYGON_OFFSET_FILL);
            //glPolygonOffset(2.0f, 2.0f);
            
            for (auto & object : sceneObjects)
            {
                if (object.castsShadow)
                {
                    shadowmapShader->uniform("u_modelMatrix", object.get_model());
                    object.draw();
                }
            }
            
            //glDisable(GL_POLYGON_OFFSET_FILL);
            shadowmapShader->unbind();
            shadowFramebuffer.unbind();
        }
        
        {
            shadowBlurFramebuffer.bind_to_draw();
            glDrawBuffer(GL_COLOR_ATTACHMENT0);
            
            gaussianBlurShader->bind();
            
            // Configured for a 7x7
            gaussianBlurShader->uniform("blurSize", 1.0f / shadowmapResolution);
            gaussianBlurShader->uniform("sigma", blurSigma);
            gaussianBlurShader->uniform("u_modelViewProj", Identity4x4);
            
            // Horizontal
            gaussianBlurShader->texture("s_blurTexure", 0, shadowDepthTexture);
            gaussianBlurShader->uniform("numBlurPixelsPerSide", 3.0f);
            gaussianBlurShader->uniform("blurMultiplyVec", float2(1.0f, 0.0f));
            fullscreen_post_quad.draw_elements();
            
            // Vertical
            gaussianBlurShader->texture("s_blurTexure", 0, shadowBlurTexture);
            gaussianBlurShader->uniform("numBlurPixelsPerSide", 3.0f);
            gaussianBlurShader->uniform("blurMultiplyVec", float2(0.0f, 1.0f));
            fullscreen_post_quad.draw_elements();

            gaussianBlurShader->unbind();
            shadowBlurFramebuffer.unbind();
        }
        
        {
            glViewport(0, 0, width, height);
            sceneShader->bind();
            
            sceneShader->uniform("u_viewProj", viewProj);
            sceneShader->uniform("u_eye", camera.get_eye_point());
            sceneShader->uniform("u_directionalLight.color", sunLight->color);
            sceneShader->uniform("u_directionalLight.direction", sunLight->direction);
            sceneShader->uniform("u_dirLightViewProjectionMat", sunLight->get_view_proj_matrix(target));
            
            sceneShader->uniform("u_spotLightViewProjectionMat[0]", spotLights[0]->get_view_proj_matrix());
            //sceneShader->uniform("u_spotLightViewProjectionMat[1]", sunLight->get_view_proj_matrix(target));

            sceneShader->uniform("u_spotLights[0].color", spotLights[0]->color);
            sceneShader->uniform("u_spotLights[0].direction", spotLights[0]->direction);
            sceneShader->uniform("u_spotLights[0].position", spotLights[0]->position);
            sceneShader->uniform("u_spotLights[0].cutoff", spotLights[0]->get_cutoff());
            sceneShader->uniform("u_spotLights[0].constantAtten", spotLights[0]->attenuation.x);
            sceneShader->uniform("u_spotLights[0].linearAtten", spotLights[0]->attenuation.y);
            sceneShader->uniform("u_spotLights[0].quadraticAtten", spotLights[0]->attenuation.z);
            
            sceneShader->uniform("u_shadowMapBias", 0.01f / shadowmapResolution);
            sceneShader->uniform("u_shadowMapTexelSize", float2(1.0f / shadowmapResolution));
            sceneShader->texture("s_directionalShadowMap", 0, shadowBlurTexture);
            
            for (auto & object : sceneObjects)
            {
                sceneShader->uniform("u_modelMatrix", object.get_model());
                sceneShader->uniform("u_modelMatrixIT", inv(transpose(object.get_model())));
                object.draw();
            }
            
            sceneShader->unbind();
        }
        
        {
            //ImGui::Checkbox("Show Shadowmap", &showShadowmap);
            ImGui::Separator();
            ImGui::SliderFloat("Near Clip", &camera.nearClip, 0.1f, 2.0f);
            ImGui::SliderFloat("Far Clip", &camera.farClip, 2.0f, 75.0f);
            ImGui::DragFloat3("Light Direction", &sunLight->direction[0], 0.1f, -1.0f, 1.0f);
            ImGui::Separator();
            ImGui::SliderFloat("Blur Sigma", &blurSigma, 0.05f, 9.0f);
            ImGui::Separator();
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        }
        
        viewA->draw(uiSurface.children[0]->bounds, int2(width, height));
        viewB->draw(uiSurface.children[1]->bounds, int2(width, height));
        //viewC->draw(uiSurface.children[2]->bounds, int2(width, height), 2);
        //viewD->draw(uiSurface.children[3]->bounds, int2(width, height), 3);
        
        gl_check_error(__FILE__, __LINE__);
        
        if (igm) igm->end_frame();
        
        glfwSwapBuffers(window);
    }
    
};
