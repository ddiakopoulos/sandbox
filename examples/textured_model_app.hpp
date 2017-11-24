#include "index.hpp"

enum class ShadingMode
{
    TEXTURED,
    MATCAP,
    WORLDSPACE_NORMAL
};

struct ExperimentalApp : public GLFWApp
{
    std::unique_ptr<gui::ImGuiInstance> igm;

    SimpleStaticMesh mesh;
    
    GlMesh fullscreen_vignette_quad;
    
    ShadingMode mode = { ShadingMode::TEXTURED };

    GlTexture2D modelDiffuseTexture;
	GlTexture2D modelNormalTexture;
	GlTexture2D modelSpecularTexture;
	GlTexture2D modelGlossTexture;
	GlTexture2D matcapTex;
    
    std::shared_ptr<GlShader> texturedModelShader;
    std::shared_ptr<GlShader> vignetteShader;
    std::shared_ptr<GlShader> matcapShader;
    std::shared_ptr<GlShader> normalDebugShader;

	ShaderMonitor shaderMonitor = { "../assets/" };
    
    GlCamera camera;
    ArcballCamera * myArcball;
    
    bool useNormal = false;
    bool useMatcap = false;
    bool useRimlight = false;
    
    ExperimentalApp() : GLFWApp(1200, 800, "Model Viewer App")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        igm.reset(new gui::ImGuiInstance(window));

        mesh.set_static_mesh(make_cube());

        texturedModelShader = shaderMonitor.watch("../assets/shaders/textured_model_vert.glsl", "../assets/shaders/textured_model_frag.glsl");
        vignetteShader = shaderMonitor.watch("../assets/shaders/vignette_vert.glsl", "../assets/shaders/vignette_frag.glsl");
        matcapShader = shaderMonitor.watch("../assets/shaders/matcap_vert.glsl", "../assets/shaders/matcap_frag.glsl");
        normalDebugShader = shaderMonitor.watch("../assets/shaders/normal_debug_vert.glsl", "../assets/shaders/normal_debug_frag.glsl");

        modelDiffuseTexture = load_image("../assets/textures/uv_checker_map/uvcheckermap_01.png");
        modelNormalTexture = load_image("../assets/textures/normal/mesh.png");

        //modelSpecularTexture = load_image("assets/textures/modular_panel/specular.png");
        //modelGlossTexture = load_image("assets/textures/modular_panel/gloss.png");

        matcapTex = load_image("../assets/textures/matcap/metal_heated.png");

        fullscreen_vignette_quad = make_fullscreen_quad();
        
        myArcball = new ArcballCamera(float2(width, height));
        
        camera.look_at({0, 0, 5}, {0, 0, 0});
        
        gl_check_error(__FILE__, __LINE__);
    }
    
    void on_window_resize(int2 size) override
    {
        
    }
    
    void on_input(const InputEvent & event) override
    {
        if (igm) igm->update_input(event);
        
        if (event.type == InputEvent::MOUSE && event.is_down())
        {
            myArcball->mouse_down(event.cursor);
        }
        else if (event.type == InputEvent::CURSOR && event.drag)
        {
            myArcball->mouse_drag(event.cursor);
            auto currentPose = mesh.get_pose();
            currentPose.orientation = normalize(qmul(myArcball->currentQuat, currentPose.orientation));
            mesh.set_pose(currentPose);
        }
    }
    
    void on_update(const UpdateEvent & e) override
    {
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
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        
        const float4x4 projectionMatrix = camera.get_projection_matrix((float) width / (float) height);
        const float4x4 viewMatrix = camera.get_view_matrix();
        const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);
        
        vignetteShader->bind();
        vignetteShader->uniform("u_noiseAmount", 0.1f);
        vignetteShader->uniform("u_screenResolution", float2(width, height));
        vignetteShader->uniform("u_backgroundColor", float3(20.f / 255.f, 20.f / 255.f, 20.f / 255.f));
        fullscreen_vignette_quad.draw_elements();
        vignetteShader->unbind();
        
        const float4x4 model = mesh.get_pose().matrix();

        if (mode == ShadingMode::TEXTURED)
        {
            texturedModelShader->bind();
            
            texturedModelShader->uniform("u_viewMatrix", viewMatrix);
            texturedModelShader->uniform("u_viewProjMatrix", viewProjectionMatrix);
            texturedModelShader->uniform("u_eyePos", camera.get_eye_point());
            
            texturedModelShader->uniform("u_ambientLight", float3(1.0f, 1.0f, 1.0f));
            
            texturedModelShader->uniform("u_rimLight.enable", int(useRimlight));
            texturedModelShader->uniform("u_rimLight.color", float3(1.0f));
            texturedModelShader->uniform("u_rimLight.power", 0.99f);
            
            texturedModelShader->uniform("u_material.diffuseIntensity", float3(1.0f, 1.0f, 1.0f));
            texturedModelShader->uniform("u_material.ambientIntensity", float3(1.0f, 1.0f, 1.0f));
            texturedModelShader->uniform("u_material.specularIntensity", float3(1.0f, 1.0f, 1.0f));
            texturedModelShader->uniform("u_material.specularPower", 128.0f);
            
            texturedModelShader->uniform("u_pointLights[0].position", float3(6, 10, -6));
            texturedModelShader->uniform("u_pointLights[0].diffuseColor", float3(1.f, 0.0f, 0.0f));
            texturedModelShader->uniform("u_pointLights[0].specularColor", float3(1.f, 1.0f, 1.0f));
            
            texturedModelShader->uniform("u_pointLights[1].position", float3(-6, 10, 6));
            texturedModelShader->uniform("u_pointLights[1].diffuseColor", float3(0.0f, 0.0f, 1.f));
            texturedModelShader->uniform("u_pointLights[1].specularColor", float3(1.0f, 1.0f, 1.f));

            texturedModelShader->uniform("u_enableDiffuseTex", 1);
            texturedModelShader->uniform("u_enableNormalTex", 1);
            texturedModelShader->uniform("u_enableSpecularTex", 0);
            texturedModelShader->uniform("u_enableEmissiveTex", 0);
            texturedModelShader->uniform("u_enableGlossTex", 0);
            
            texturedModelShader->texture("u_diffuseTex", 0, modelDiffuseTexture, GL_TEXTURE_2D);
            texturedModelShader->texture("u_normalTex", 1, modelNormalTexture, GL_TEXTURE_2D);
            //texturedModelShader->texture("u_specularTex", 2, modelSpecularTexture, GL_TEXTURE_2D);
            //texturedModelShader->texture("u_glossTex", 3, modelGlossTexture, GL_TEXTURE_2D);
            
            texturedModelShader->uniform("u_modelMatrix", model);
            texturedModelShader->uniform("u_modelMatrixIT", inv(transpose(model)));
            mesh.draw();
            
            texturedModelShader->unbind();
        }
        else if (mode == ShadingMode::WORLDSPACE_NORMAL)
        {
			normalDebugShader->bind();
			normalDebugShader->uniform("u_viewProj", viewProjectionMatrix);
			normalDebugShader->uniform("u_modelMatrix", model);
			normalDebugShader->uniform("u_modelMatrixIT", inv(transpose(model)));
			mesh.draw();
            normalDebugShader->unbind();
        }

        else if (mode == ShadingMode::MATCAP)
        {
            matcapShader->bind();
            matcapShader->uniform("u_viewProj", viewProjectionMatrix);
            matcapShader->uniform("u_modelMatrix", model);
            matcapShader->uniform("u_modelViewMatrix", mul(viewMatrix, model));
            matcapShader->uniform("u_modelMatrixIT", get_rotation_submatrix(inv(transpose(model))));
            matcapShader->texture("u_matcapTexture", 0, matcapTex, GL_TEXTURE_2D);
            mesh.draw();
            matcapShader->unbind();
        }
        
        if (igm) igm->begin_frame();
        ImGui::Checkbox("Apply Rimlight", &useRimlight);
        if (ImGui::Button("Textured")) { mode = ShadingMode::TEXTURED; }
        if (ImGui::Button("Matcap")) { mode = ShadingMode::MATCAP; }
        if (ImGui::Button("Normal")) { mode = ShadingMode::WORLDSPACE_NORMAL; }
        if (igm) igm->end_frame();
        
        gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
    }
    
};