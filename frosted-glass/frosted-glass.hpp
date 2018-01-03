#include "index.hpp"
#include "gl-gizmo.hpp"

struct blur_chain
{
    GlMesh quad;
    GlShader blur;

    float blurSigma = 4.0f;
    int blurPixelsPerSide = 2;

    struct target
    {
        GlFramebuffer framebuffer;
        GlTexture2D colorAttachment0;
        GlTexture2D colorAttachment1;
        float2 size;
    };

    std::vector<target> targets;

    blur_chain(const float2 size)
    {
        quad = make_fullscreen_quad();

        const std::vector<float2> sizes = {
            { size.x / 2,  size.y / 2 },
            { size.x / 4,  size.y / 4 },
            { size.x / 8,  size.y / 8 },
            { size.x / 16, size.y / 16 },
            { size.x / 32, size.y / 32 }
        };

        targets.resize(5);

        for (int i = 0; i < targets.size(); ++i)
        {
            auto & target = targets[i];

            target.size = sizes[i];

            target.colorAttachment0.setup(target.size.x, target.size.y, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTextureParameteriEXT(target.colorAttachment0, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTextureParameteriEXT(target.colorAttachment0, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

            target.colorAttachment1.setup(target.size.x, target.size.y, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTextureParameteriEXT(target.colorAttachment1, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTextureParameteriEXT(target.colorAttachment1, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

            glNamedFramebufferTexture2DEXT(target.framebuffer, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.colorAttachment0, 0);
            glNamedFramebufferTexture2DEXT(target.framebuffer, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, target.colorAttachment1, 0);

            target.framebuffer.check_complete();
        }

        blur = GlShader(read_file_text("../assets/shaders/renderer/gaussian_blur_vert.glsl"), read_file_text("../assets/shaders/renderer/gaussian_blur_frag.glsl"));

        gl_check_error(__FILE__, __LINE__);
    }

    void execute(const GlTexture2D & colorTexture)
    {
        for (int i = 0; i < targets.size(); ++i)
        {
            auto & target = targets[i];

            glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
            glViewport(0, 0, target.size.x, target.size.y);

            blur.bind();

            blur.uniform("u_modelViewProj", Identity4x4);
            blur.uniform("sigma", blurSigma);
            blur.uniform("numBlurPixelsPerSide", float(blurPixelsPerSide));

            // Horizontal pass - output to attachment 0 
            glDrawBuffer(GL_COLOR_ATTACHMENT0);
            blur.uniform("blurSize", 1.f / target.size.x);
            blur.uniform("blurMultiplyVec", float2(1.0f, 0.0f));
            blur.texture("s_blurTexure", 0,  (i == 0) ? colorTexture : targets[i - 1].colorAttachment1, GL_TEXTURE_2D);
            quad.draw_elements();

            // Vertical pass - output to attachment 1
            glDrawBuffer(GL_COLOR_ATTACHMENT1);
            blur.uniform("blurSize", 1.f / target.size.y);
            blur.uniform("blurMultiplyVec", float2(0.0f, 1.0f));
            blur.texture("s_blurTexure", 0, target.colorAttachment0, GL_TEXTURE_2D); 
            quad.draw_elements();

            blur.unbind();
 
            gl_check_error(__FILE__, __LINE__);
        }
    }
};

struct shader_workbench : public GLFWApp
{
    GlCamera cam;
    FlyCameraController flycam;
    ShaderMonitor shaderMonitor{ "../assets/" };

    std::unique_ptr<gui::imgui_wrapper> igm;
    GlGpuTimer gpuTimer;
    std::unique_ptr<GlGizmo> gizmo;

    GlFramebuffer sceneFramebuffer;
    GlTexture2D sceneColor;
    GlTexture2D sceneDepth;

    GlShader skyShader;
    GlShader glassShader;
    GlShader texturedShader;

    GlMesh skyMesh;
    GlMesh cube;
    GlMesh floorMesh;
    GlMesh glassSurface;

    GlTexture2D glassTex;
    GlTexture2D cubeTex;
    GlTexture2D floorTex;

    std::unique_ptr<CubemapCamera> cubemapCam;

    int glassTextureSelection = 1;
    bool showDebug = false;
    bool animateCube = false;

    float angle = 0.f;

    auto_layout uiSurface;
    std::vector<std::shared_ptr<GLTextureView>> views;

    std::unique_ptr<blur_chain> post;

    shader_workbench();
    ~shader_workbench();

    virtual void on_window_resize(int2 size) override;
    virtual void on_input(const InputEvent & event) override;
    virtual void on_update(const UpdateEvent & e) override;
    virtual void on_draw() override;
};