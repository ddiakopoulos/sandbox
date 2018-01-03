#include "index.hpp"
#include "gl-gizmo.hpp"

struct PointLight
{
    float3 position;
    float3 color;
};

struct shader_workbench : public GLFWApp
{
    GlCamera cam;
    FlyCameraController flycam;
    ShaderMonitor shaderMonitor{ "../assets/" };
    std::unique_ptr<gui::imgui_wrapper> igm;
    GlGpuTimer gpuTimer;
    std::unique_ptr<GlGizmo> gizmo;

    GlTexture2D portalCameraRGB;
    GlTexture2D portalCameraDepth;
    GlFramebuffer portalFramebuffer;

    GlMesh fullscreen_quad, capsuleMesh, portalMesh, frustumMesh;
    std::shared_ptr<GlShader> basicShader;
    GlShader billboardShader, litShader;
    std::unique_ptr<RenderableGrid> grid;

    std::vector<PointLight> lights;
    std::vector<Pose> objects;

    Pose sourcePose;
    Pose destinationPose;
    Pose portalCameraPose;
    tinygizmo::rigid_transform destination;

    float elapsedTime{ 0 };

    shader_workbench();
    ~shader_workbench();

    virtual void on_window_resize(int2 size) override;
    virtual void on_input(const InputEvent & event) override;
    virtual void on_update(const UpdateEvent & e) override;
    virtual void on_draw() override;
};