#include "index.hpp"
#include "gl-gizmo.hpp"

struct gl_material_projector
{
    float4x4 viewMatrix;

    std::shared_ptr<GlShader> shader;
    std::shared_ptr<GlTexture2D> cookieTexture;
    std::shared_ptr<GlTexture2D> gradientTexture;

    // http://developer.download.nvidia.com/CgTutorial/cg_tutorial_chapter09.html
    float4x4 get_view_projection_matrix(bool isOrthographic = false)
    {
        if (isOrthographic)
        {
            const float halfSize = 1.0 * 0.5f;
            return mul(make_orthographic_matrix(-halfSize, halfSize, -halfSize, halfSize, -halfSize, halfSize), viewMatrix);
        }
        return mul(make_perspective_matrix(to_radians(45.f), 1.0f, 0.1f, 16.f), viewMatrix);
    }

    float4x4 get_projector_matrix(bool isOrthographic = false)
    {
        // Bias matrix is a constant.
        // It performs a linear transformation to go from the [–1, 1]
        // range to the [0, 1] range. Having the coordinates in the [0, 1]
        // range is necessary for the values to be used as texture coordinates.
        const float4x4 biasMatrix = {
            { 0.5f,  0.0f,  0.0f,  0.0f },
            { 0.0f,  0.5f,  0.0f,  0.0f },
            { 0.0f,  0.0f,  0.5f,  0.0f },
            { 0.5f,  0.5f,  0.5f,  1.0f }
        };
        return mul(get_view_projection_matrix(false), biasMatrix);
    }
};

struct shader_workbench : public GLFWApp
{
    GlCamera cam;
    FlyCameraController flycam;
    ShaderMonitor shaderMonitor{ "../assets/" };
    std::unique_ptr<gui::ImGuiManager> igm;
    GlGpuTimer gpuTimer;
    std::unique_ptr<GlGizmo> gizmo;

    GlTexture2D rustyTexture, topTexture;

    gl_material_projector projector;

    float elapsedTime{ 0 };

    std::shared_ptr<GlShader> terrainScan;
    std::shared_ptr<GlShader> triplanarTexture;
    std::shared_ptr<GlShader> normalDebug;

    GlMesh terrainMesh;

    shader_workbench();
    ~shader_workbench();

    virtual void on_window_resize(int2 size) override;
    virtual void on_input(const InputEvent & event) override;
    virtual void on_update(const UpdateEvent & e) override;
    virtual void on_draw() override;
};