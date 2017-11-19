#include "index.hpp"
#include "gl-gizmo.hpp"

// http://www.bfilipek.com/2014/04/flexible-particle-system-start.html?m=1

struct particle
{
    float3 position;
    float3 velocity;
    float size;
    float lifeMs;
    bool isDead = false;
};

class particle_system
{
    std::vector<particle> particles;
    std::vector<float4> instances;
    GlBuffer vertexBuffer, instanceBuffer;
    size_t trailCount = 0;
public:
    particle_system(size_t trailCount);
    void update(float dt, const float3 & gravityVec);
    void add(const float3 & position, const float3 & velocity, float size, float lifeMs);
    void draw(const float4x4 & viewMat, const float4x4 & projMat, GlShader & shader, GlTexture2D & outerTex, GlTexture2D & innerTex);
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

    std::unique_ptr<particle_system> particleSystem;
    GlShader particleShader;
    GlTexture2D outerTex;
    GlTexture2D innerTex;

    UpdateEvent lastUpdate;
    float elapsedTime{ 0 };

    shader_workbench();
    ~shader_workbench();

    virtual void on_window_resize(int2 size) override;
    virtual void on_input(const InputEvent & event) override;
    virtual void on_update(const UpdateEvent & e) override;
    virtual void on_draw() override;
};