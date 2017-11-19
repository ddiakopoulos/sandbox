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

struct particle_modifier
{
    virtual void update(std::vector<particle> & particles, float dt) = 0;
};

struct gravity_modifier final : public particle_modifier
{
    float3 gravityVec;
    gravity_modifier(const float3 gravityVec) : gravityVec(gravityVec) { }
    void update(std::vector<particle> & particles, float dt) override
    {
        for (auto & p : particles)
        {
            p.velocity += gravityVec * dt;
        }
    }
};

struct point_gravity_modifier final : public particle_modifier
{
    float3 position;
    float strength;
    float maxStrength;
    float radiusSquared;

    point_gravity_modifier(float3 & position, float strength, float maxStrength, float radius)
        : position(position), strength(strength), maxStrength(maxStrength), radiusSquared(radius * radius) { }

    void update(std::vector<particle> & particles, float dt) override
    {
        for (auto & p : particles)
        {
            float3 distance = position - p.position;
            float distSqr = length2(distance);
            if (distSqr > radiusSquared) return;
            float force = strength / distSqr;
            force = force > maxStrength ? maxStrength : force;
            p.velocity += normalize(distance) * force;
        }
    }
};

struct damping_modifier final : public particle_modifier
{
    float damping;
    damping_modifier(const float damping) : damping(damping) { }
    void update(std::vector<particle> & particles, float dt) override
    {
        for (auto & p : particles)
        {
            p.velocity *= std::pow(damping, dt);
        }
    }
};

struct ground_modifier final : public particle_modifier
{
    Plane ground;
    ground_modifier(const Plane p) : ground(p) { }
    void update(std::vector<particle> & particles, float dt) override
    {
        for (auto & p : particles)
        {
            float reflectedVelocity = dot(ground.get_normal(), p.velocity);
            if (dot(ground.equation, float4(p.position, 1)) < 0.f && reflectedVelocity < 0.f)
            {
                p.velocity -= ground.get_normal() * (reflectedVelocity * 2.0f);
            }
        }
    }
};

class particle_system
{
    std::vector<particle> particles;
    std::vector<float4> instances;
    GlBuffer vertexBuffer, instanceBuffer;
    std::vector<std::unique_ptr<particle_modifier>> particleModifiers;
    size_t trailCount = 0;
public:
    particle_system(size_t trailCount);
    void update(float dt, const float3 & gravityVec);
    void add_modifier(std::unique_ptr<particle_modifier> modifier);
    void add(const float3 & position, const float3 & velocity, float size, float lifeMs);
    void draw(const float4x4 & viewMat, const float4x4 & projMat, GlShader & shader, GlTexture2D & outerTex, GlTexture2D & innerTex);
};

struct particle_emitter
{
    Pose pose;
    UniformRandomGenerator gen;
    virtual void emit(particle_system & system) = 0;
};

struct point_emitter final : public particle_emitter
{
    void emit(particle_system & system) override 
    {
        for (int i = 0; i < 12; ++i)
        {
            auto v1 = gen.random_float(-.5f, +.5f);
            auto v2 = gen.random_float(0.5f, 2.f);
            auto v3 = gen.random_float(-.5f, +.5f);
            system.add(pose.position, float3(v1, v2, v3), gen.random_float(0.05f, 0.2f), 4.f);
        }
    }
};

struct cube_emitter final : public particle_emitter
{
    void emit(particle_system & system) override { }
};

struct sphere_emitter final : public particle_emitter
{
    void emit(particle_system & system) override { }
};

struct plane_emitter_2d final : public particle_emitter
{
    void emit(particle_system & system) override { }
};

struct circle_emitter_2d final : public particle_emitter
{
    void emit(particle_system & system) override { }
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
    std::unique_ptr<gravity_modifier> gravityModifier;
    point_emitter pointEmitter;

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