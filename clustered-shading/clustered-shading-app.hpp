#include "index.hpp"
#include "gl-gizmo.hpp"

namespace uniforms
{
    static const size_t MAX_POINT_LIGHTS = 1024;

    struct point_light
    {
        ALIGNED(16) float4 positionRadius;
        ALIGNED(16) float4 colorIntensity;
    };

    struct clustered_lighting_buffer
    {
        static const int binding = 7;
        point_light lights[MAX_POINT_LIGHTS];
    };
};

//  "2D Polyhedral Bounds of a Clipped, Perspective - Projected 3D Sphere"
inline Bounds3D sphere_for_axis(const float3 & axis, const float3 & sphereCenter, const float sphereRadius, const float zNearClipCamera)
{
    const bool sphereClipByZNear = !((sphereCenter.z + sphereRadius) < zNearClipCamera);
    const float2 projectedCenter = float2(dot(axis, sphereCenter), sphereCenter.z);
    const float tSquared = dot(projectedCenter, projectedCenter) - (sphereRadius * sphereRadius);

    float t, cLength, cosTheta, sintheta;

    const bool outsideSphere = (tSquared > 0);

    if (outsideSphere)
    {
        // cosTheta, sinTheta of angle between the projected center (in a-z space) and a tangent
        t = sqrt(tSquared);
        cLength = length(projectedCenter);
        cosTheta = t / cLength;
        sintheta = sphereRadius / cLength;
    }

    // Square root of the discriminant; NaN(and unused) if the camera is in the sphere
    float sqrtPart = 0.f;
    if (sphereClipByZNear)
    {
        sqrtPart = std::sqrt((sphereRadius * sphereRadius) - ((zNearClipCamera - projectedCenter.y) * (zNearClipCamera - projectedCenter.y)));
    }

    float2 bounds[2]; // in the a-z reference frame

    sqrtPart *= -1.f;
    for (int i = 0; i < 2; ++i)
    {
        if (tSquared >  0)
        {
            const float2x2 rotator = { { cosTheta, -sintheta },{ sintheta, cosTheta } };
            const float2 rotated = normalize(mul(rotator, projectedCenter));
            bounds[i] = cosTheta * mul(rotator, projectedCenter);
        }

        if (sphereClipByZNear && (!outsideSphere || bounds[i].y > zNearClipCamera))
        {
            bounds[i].x = projectedCenter.x + sqrtPart;
            bounds[i].y = zNearClipCamera;
        }
        sintheta *= -1.f;
        sqrtPart *= -1.f;
    }

    Bounds3D boundsViewSpace;
    boundsViewSpace._min = bounds[0].x * axis;
    boundsViewSpace._min.z = bounds[0].y;
    boundsViewSpace._max = bounds[1].x * axis;
    boundsViewSpace._max.z = bounds[1].y;

    return boundsViewSpace;
}

struct shader_workbench : public GLFWApp
{
    GlCamera debugCamera;
    FlyCameraController cameraController;
    ShaderMonitor shaderMonitor{ "../assets/" };

    std::unique_ptr<gui::ImGuiInstance> igm;
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
    std::vector<float3> randomPositions;

    std::vector<uniforms::point_light> lights;

    UpdateEvent lastUpdate;
    float elapsedTime{ 0 };

    shader_workbench();
    ~shader_workbench();

    virtual void on_window_resize(int2 size) override;
    virtual void on_input(const InputEvent & event) override;
    virtual void on_update(const UpdateEvent & e) override;
    virtual void on_draw() override;
};