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

struct vortex_modifier final : public particle_modifier
{
    float3 position;
    float3 direction;
    float angle;
    float strength;
    float radius;
    float damping;

    vortex_modifier(float3 & position, float3 & direction, float angle, float strenth, float radius, float damping)
        : position(position), direction(direction), angle(angle), strength(strenth), radius(radius), damping(damping) { }

    void update(std::vector<particle> & particles, float dt) override
    {
        for (auto & p : particles)
        {
            float3 relativeDistance = p.position - position;
            float distance = length(relativeDistance);

            //if (distance > radius) return;

            float3 force;
            float forceStrength = 0.f;

            forceStrength = strength * (radius - distance) / radius;
            force = cross(direction, relativeDistance);

            auto rotator = make_rotation_matrix({ 0, 0, 1 }, angle);
            force = transform_vector(rotator, force);
            force *= forceStrength;

            //force -= p.velocity * length(p.velocity) * damping;

            p.velocity += force;

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
    void draw(const float4x4 & viewMat, const float4x4 & projMat, GlShader & shader, GlTexture2D & outerTex, GlTexture2D & innerTex, float time);
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
    Bounds3D localBounds;
    cube_emitter(Bounds3D local) : localBounds(local) { }
    void emit(particle_system & system) override 
    { 
        float3 min = pose.transform_coord(-(localBounds.size() * 0.5f));
        float3 max = pose.transform_coord(+(localBounds.size() * 0.5f));

        for (int i = 0; i < 1; ++i)
        {
            auto v1 = gen.random_float(min.x, max.x);
            auto v2 = gen.random_float(min.y, max.y);
            auto v3 = gen.random_float(min.z, max.z);
            system.add(float3(v1, v2, v3), float3(0, 1, 0), gen.random_float(0.05f, 0.2f), 4.f);
        }
    }
};

struct sphere_emitter final : public particle_emitter
{
    Bounds3D localBounds;
    sphere_emitter(Bounds3D local) : localBounds(local) { }
    void emit(particle_system & system) override 
    {
        for (int i = 0; i < 12; ++i)
        {
            float u = gen.random_float(0, 1) * float(ANVIL_PI);
            float v = gen.random_float(0, 1) * float(ANVIL_TAU);
            float3 normal = cartsesian_coord(u, v, 1.f);
            float3 point = pose.transform_coord(normal);
            system.add(point, normal * 0.5f, 0.1f, 4.f);
        }
    }
};

struct plane_emitter_2d final : public particle_emitter
{
    Bounds2D localBounds;
    plane_emitter_2d(Bounds2D local) : localBounds(local) { }
    void emit(particle_system & system) override 
    { 
        for (int i = 0; i < 3; ++i)
        {
            float2 halfExtents = localBounds.size() * 0.5f;
            float w = gen.random_float(-halfExtents.x, halfExtents.x);
            float h = gen.random_float(-halfExtents.y, halfExtents.y);
            float3 point = pose.transform_coord(float3(w, 0, h));
            system.add(point, float3(0, 1, 0), 0.1f, 4.f);
        }
    }
};

struct circle_emitter_2d final : public particle_emitter
{
    Bounds2D localBounds;
    circle_emitter_2d(Bounds2D local) : localBounds(local) { }
    void emit(particle_system & system) override 
    { 
        float2 size = localBounds.size();
        float radius = 0.5f * std::sqrt(size.x * size.x + size.y * size.y);
        radius = gen.random_float(0, radius);
        for (int i = 0; i < 3; ++i)
        {
            float ang = gen.random_float_sphere();
            float w = std::cos(ang) * radius;
            float h = std::sin(ang) * radius;
            float3 point = pose.transform_coord(float3(w, 0, h));
            system.add(point, float3(0, 1, 0), 0.1f, 4.f);
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

    SimpleTimer timer;

    std::shared_ptr<GlShader> basicShader;
    std::unique_ptr<RenderableGrid> grid;

    std::unique_ptr<particle_system> particleSystem;
    std::unique_ptr<gravity_modifier> gravityModifier;

    point_emitter pointEmitter;
    cube_emitter cubeEmitter = { Bounds3D(float3(-1.f), float3(1.f)) };
    sphere_emitter sphereEmitter = { Bounds3D(float3(-1.f), float3(1.f)) };
    plane_emitter_2d planeEmitter = { Bounds2D(float2(-1.f), float2(1.f)) };
    circle_emitter_2d circleEmitter = { Bounds2D(float2(-1.f), float2(1.f)) };

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