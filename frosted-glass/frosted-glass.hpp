#include "index.hpp"
#include "gl-gizmo.hpp"

struct post_chain
{
    std::vector<GlTexture2D> levelTex1;
    std::vector<GlTexture2D> levelTex2;
    std::vector<GlTexture2D> levelBuf;

    GlMesh quad;
    float2 size;

    GlShader blur;

    float blurSigma = 4.0f;
    int blurPixelsPerSide = 2;

    post_chain(const float2 size) : size(size)
    {
        quad = make_fullscreen_quad();

        levelTex1.resize(5);
        levelTex2.resize(5);
        levelBuf.resize(5);

        levelTex1[0].setup(size.x / 2,  size.y / 2, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTextureParameteriEXT(levelTex1[0], GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTextureParameteriEXT(levelTex1[0], GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

        levelTex1[1].setup(size.x / 4,  size.y / 4, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTextureParameteriEXT(levelTex1[1], GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTextureParameteriEXT(levelTex1[1], GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

        levelTex1[2].setup(size.x / 8,  size.y / 8, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTextureParameteriEXT(levelTex1[2], GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTextureParameteriEXT(levelTex1[2], GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

        levelTex1[3].setup(size.x / 16, size.y / 16, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTextureParameteriEXT(levelTex1[3], GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTextureParameteriEXT(levelTex1[3], GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

        levelTex1[4].setup(size.x / 32, size.y / 32, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTextureParameteriEXT(levelTex1[4], GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTextureParameteriEXT(levelTex1[4], GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

        levelTex2[0].setup(size.x / 2, size.y / 2, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTextureParameteriEXT(levelTex2[0], GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTextureParameteriEXT(levelTex2[0], GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

        levelTex2[1].setup(size.x / 4, size.y / 4, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTextureParameteriEXT(levelTex2[1], GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTextureParameteriEXT(levelTex2[1], GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

        levelTex2[2].setup(size.x / 8, size.y / 8, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTextureParameteriEXT(levelTex2[2], GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTextureParameteriEXT(levelTex2[2], GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

        levelTex2[3].setup(size.x / 16, size.y / 16, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTextureParameteriEXT(levelTex2[3], GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTextureParameteriEXT(levelTex2[3], GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

        levelTex2[4].setup(size.x / 32, size.y / 32, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTextureParameteriEXT(levelTex2[4], GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTextureParameteriEXT(levelTex2[4], GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

        glNamedFramebufferTexture2DEXT(levelBuf[0], GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, levelTex1[0], 0);
        glNamedFramebufferTexture2DEXT(levelBuf[1], GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, levelTex1[1], 0);
        glNamedFramebufferTexture2DEXT(levelBuf[2], GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, levelTex1[2], 0);
        glNamedFramebufferTexture2DEXT(levelBuf[3], GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, levelTex1[3], 0);
        glNamedFramebufferTexture2DEXT(levelBuf[4], GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, levelTex1[4], 0);

        glNamedFramebufferTexture2DEXT(levelBuf[0], GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, levelTex2[0], 0);
        glNamedFramebufferTexture2DEXT(levelBuf[1], GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, levelTex2[1], 0);
        glNamedFramebufferTexture2DEXT(levelBuf[2], GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, levelTex2[2], 0);
        glNamedFramebufferTexture2DEXT(levelBuf[3], GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, levelTex2[3], 0);
        glNamedFramebufferTexture2DEXT(levelBuf[4], GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, levelTex2[4], 0);

        blur = GlShader(read_file_text("../assets/shaders/renderer/gaussian_blur_vert.glsl"), read_file_text("../assets/shaders/renderer/gaussian_blur_frag.glsl"));

        gl_check_error(__FILE__, __LINE__);
    }

    void execute(GlTexture2D & colorTexture)
    {
        std::vector<float3> downsampleTargets = { 
            { 0, size.x / 2,  size.y / 2 },
            { 1, size.x / 4,  size.y / 4 },
            { 2, size.x / 8,  size.y / 8 },
            { 3, size.x / 16, size.y / 16 },
            { 4, size.x / 32, size.y / 32 } 
        };

        auto downsample = [&](const int idx, const float2 targetSize)
        {
            //std::cout << "Idx: " << idx << std::endl;

            {
                glBindFramebuffer(GL_FRAMEBUFFER, levelBuf[idx]);
                glViewport(0, 0, targetSize.x, targetSize.y);

                blur.bind();

                blur.uniform("u_modelViewProj", Identity4x4);
                blur.uniform("sigma", blurSigma);
                blur.uniform("numBlurPixelsPerSide", float(blurPixelsPerSide));

                glDrawBuffer(GL_COLOR_ATTACHMENT0);

                // Horizontal pass - output to attachment 0 
                blur.uniform("blurSize", 1.f / targetSize.x);
                blur.uniform("blurMultiplyVec", float2(1.0f, 0.0f));
                blur.texture("s_blurTexure", 0,  (idx == 0) ? colorTexture : levelTex2[idx - 1], GL_TEXTURE_2D);
                quad.draw_elements();

                glDrawBuffer(GL_COLOR_ATTACHMENT1);

                // Vertical pass - output to attachment 1
                blur.uniform("blurSize", 1.f / targetSize.y);
                blur.uniform("blurMultiplyVec", float2(0.0f, 1.0f));
                blur.texture("s_blurTexure", 0, levelTex1[idx], GL_TEXTURE_2D); 
                quad.draw_elements();

                blur.unbind();
            }

            gl_check_error(__FILE__, __LINE__);

        };

        for (auto target : downsampleTargets)
        {
            downsample(target.x, float2(target.y, target.z));
        }
        //std::cout << "End Pass --- " << std::endl;
    }
};

struct shader_workbench : public GLFWApp
{
    GlCamera cam;
    FlyCameraController flycam;
    ShaderMonitor shaderMonitor{ "../assets/" };

    std::unique_ptr<gui::ImGuiInstance> igm;
    GlGpuTimer gpuTimer;
    std::unique_ptr<GlGizmo> gizmo;
    GlShader basicShader;
    GlShader glassShader;

    GlTexture2D glassNormal;

    float elapsedTime{ 0 };

    GlMesh glassSurface;
    GlMesh cube;

    GlTexture2D sceneColor;
    GlTexture2D sceneDepth;
    GlFramebuffer sceneFramebuffer;

    GlMesh skyMesh;
    GlShader skyShader;

    GlShader texturedShader;
    GlTexture2D cubeTex;

    GlMesh floorMesh;
    GlTexture2D floorTex;

    bool showDebug = false;

    auto_layout uiSurface;
    std::vector<std::shared_ptr<GLTextureView>> views;

    std::unique_ptr<post_chain> post;

    shader_workbench();
    ~shader_workbench();

    virtual void on_window_resize(int2 size) override;
    virtual void on_input(const InputEvent & event) override;
    virtual void on_update(const UpdateEvent & e) override;
    virtual void on_draw() override;
};