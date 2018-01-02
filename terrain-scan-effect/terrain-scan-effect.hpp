// This sample originally existed as a workbench project @ f1ea9e8d13992bce5d48ce306a24031630e55095

#include "index.hpp"
#include "gl-gizmo.hpp"

struct shader_workbench : public GLFWApp
{
    GlCamera cam;
    FlyCameraController flycam;
    ShaderMonitor shaderMonitor{ "../assets/" };

    std::unique_ptr<gui::ImGuiInstance> igm;
    GlGpuTimer gpuTimer;
    std::unique_ptr<GlGizmo> gizmo;
    tinygizmo::rigid_transform destination;

    GlFramebuffer sceneFramebuffer;
    GlTexture2D sceneColorTexture;
    GlTexture2D sceneDepthTexture;

    GlTexture2D grassTexture;
    GlTexture2D rockTexture;

    GlMesh skyMesh;
    GlMesh terrainMesh;
    GlMesh fullscreenQuad;

    GlShader scanningEffect;
    GlShader triplanarTerrain;
    GlShader skyShader;

    float elapsedTime{ 0.f };

    float3 triplanarTextureScale{ 0.25f };

    float ringDiameter{ 1.0f };
    float ringEdgeSize{ 5.0f };
    float ringEdgeSharpness{ 8.0f };

    float4 leadColor{ 0.8f, 0.6f, 0.3f, 0 };
    float4 midColor{ 0.975f, 0.78f, 0.366f, 0 };
    float4 trailColor{ 1.f, 0.83f, 1, 0 };
    float4 hbarColor{ 0.05, 0.05, 0.05, 0 };

    SimpleTweenPlayer animator;

    shader_workbench();
    ~shader_workbench();

    virtual void on_window_resize(int2 size) override;
    virtual void on_input(const InputEvent & event) override;
    virtual void on_update(const UpdateEvent & e) override;
    virtual void on_draw() override;
};