#include "index.hpp"
#include "gl-gizmo.hpp"
#include "clustered-shading.hpp"

struct shader_workbench : public GLFWApp
{
    GlCamera debugCamera;
    FlyCameraController cameraController;
    ShaderMonitor shaderMonitor{ "../assets/" };

    std::unique_ptr<gui::imgui_wrapper> igm;
    std::unique_ptr<GlGizmo> gizmo;
    tinygizmo::rigid_transform xform;

    UniformRandomGenerator rand;

    GlGpuTimer renderTimer;
    SimpleTimer clusterCPUTimer;

    GlShader basicShader;
    GlShader wireframeShader;
    GlShader clusteredShader;

    std::unique_ptr<RenderableGrid> grid;

    GlMesh sphereMesh;
    GlMesh floor;
    GlMesh torusKnot;
    std::vector<float4> randomPositions;

    std::vector<uniforms::point_light> lights;

    UpdateEvent lastUpdate;
    float elapsedTime{ 0 };

    shader_workbench();
    ~shader_workbench();

    std::vector<float> angle;

    virtual void on_window_resize(int2 size) override;
    virtual void on_input(const InputEvent & event) override;
    virtual void on_update(const UpdateEvent & e) override;
    virtual void on_draw() override;

    void regenerate_lights(size_t numLights);
};