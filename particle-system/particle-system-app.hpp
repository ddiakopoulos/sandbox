#include "index.hpp"
#include "gl-gizmo.hpp"

struct particle
{
    float3 position;
    float3 velocity;
    float size;
};

class particle_system
{
    std::vector<particle> particles;
    std::vector<float4> instances;
    GlBuffer vertexBuffer, instanceBuffer;
};

struct shader_workbench : public GLFWApp
{
    GlCamera cam;
    FlyCameraController flycam;
    ShaderMonitor shaderMonitor{ "../assets/" };
    std::unique_ptr<gui::ImGuiManager> igm;
    GlGpuTimer gpuTimer;
    std::unique_ptr<GlGizmo> gizmo;

    std::shared_ptr<GlShader> basicShader;
    std::unique_ptr<RenderableGrid> grid;

    float elapsedTime{ 0 };

    shader_workbench();
    ~shader_workbench();

    virtual void on_window_resize(int2 size) override;
    virtual void on_input(const InputEvent & event) override;
    virtual void on_update(const UpdateEvent & e) override;
    virtual void on_draw() override;
};