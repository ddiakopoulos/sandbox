#include "index.hpp"
#include "holo-scan-effect.hpp"
#include "terrain-scan-effect.hpp"
#include "fade-to-skybox.hpp"
#include "cheap-subsurface-scattering.hpp"
#include "area-light-ltc.hpp"
#include "lab-teleportation-sphere.hpp"
#include "triplanar-terrain.hpp"

struct shader_workbench : public GLFWApp
{
    GlCamera cam;
    FlyCameraController flycam;
    ShaderMonitor shaderMonitor{ "../assets/" };
    std::unique_ptr<gui::ImGuiManager> igm;
    GlGpuTimer gpuTimer;

    GlTexture2D sceneColorTexture;
    GlTexture2D sceneDepthTexture;
    GlFramebuffer sceneFramebuffer;

    float elapsedTime{ 0 };

    float scanDistance{ 1.0f };
    float scanWidth{ 5.0f };
    float leadSharp{ 8.0f };
    float4 leadColor{ 0.8f, 0.6f, 0.3f, 0};
    float4 midColor{ 0.975f, 0.78f, 0.366f, 0 };
    float4 trailColor{ 1.f, 0.83f, 1, 0 };
    float4 hbarColor{ 0.05, 0.05, 0.05, 0 };

    std::shared_ptr<GlShader> holoScanShader;
    std::shared_ptr<GlShader> normalDebug;

    GlMesh terrainMesh, fullscreenQuad;

    shader_workbench();
    ~shader_workbench();

    virtual void on_window_resize(int2 size) override;
    virtual void on_input(const InputEvent & event) override;
    virtual void on_update(const UpdateEvent & e) override;
    virtual void on_draw() override;
};