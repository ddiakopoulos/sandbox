// Almost a direct port of Cinder's Arcball (https://github.com/cinder/Cinder/blob/master/include/cinder/Arcball.h) (BSD)

#ifndef arcball_camera_h
#define arcball_camera_h

#include "glfw_app.hpp"
#include "linear_algebra.hpp"
#include "math_util.hpp"
#include "geometric.hpp"
#include "util.hpp"
#include "geometry.hpp"
#include "GlShared.hpp"
#include "ellipse_math.hpp"

using namespace math;
using namespace util;
using namespace gfx;

inline float angle_from_quat(const float4 & quat)
{
    return acos(quat.w) * 2.0f;
}

inline float3 axis_from_quat(const float4 & quat)
{
    auto tmp1 = 1.0f - quat.w * quat.w;
    if (tmp1 <= 0.0f )
        return float3(0, 0, 1);
    auto tmp2 = 1.0f / sqrt(tmp1);
    return float3(quat.x * tmp2, quat.y * tmp2, quat.z * tmp2);
}

class Arcball
{
    // Force sphere point to be perpendicular to axis
    float3 constrain_to_axis(const float3 & loose, const float3 & axis)
    {
        float norm;
        float3 onPlane = loose - axis * dot(axis, loose);
        norm = lengthL2(onPlane);
        
        if (norm > 0.0f)
        {
            if (onPlane.z < 0.0f) onPlane = -onPlane;
            return (onPlane * (1.0f / std::sqrt(norm)));
        }
        
        if (dot(axis, float3(0, 0, 1)) < 0.0001f) onPlane = float3(1, 0, 0);
        else onPlane = normalize(float3(-axis.y, axis.x, 0));
        
        return onPlane;
    }
    
    bool useConstraints;
    
    GlCamera * camera;
    
    float2 initialMousePos;
    float4 currentQuat, initialQuat;
    Sphere arcballSphere;
    
public:
    
    float3 fromVector, toVector, axisConstraint;
    
    Arcball() : camera(nullptr), useConstraints(false)
    {
        disable_constraints();
    }
    
    Arcball(GlCamera * camera, const Sphere & sphere) : camera(camera), useConstraints(false), arcballSphere(sphere)
    {
    }
    
    void mouse_down(const InputEvent & event)
    {
        mouse_down(event.cursor, event.windowSize);
    }
    
    void mouse_down(const float2 &mousePos, const int2 &windowSize)
    {
        initialMousePos = mousePos;
        initialQuat = currentQuat;
        float temp;
        mouse_on_sphere(initialMousePos, windowSize, &fromVector, &temp);
    }
    
    void mouse_drag(const InputEvent & event)
    {
        mouse_drag(event.cursor, event.windowSize);
    }
    
    void mouse_drag(const float2 & mousePos, const int2 & windowSize)
    {
        float addition;
        mouse_on_sphere(mousePos, windowSize, &toVector, &addition);
        float3 from = fromVector, to = toVector;
        
        if (useConstraints)
        {
            from = constrain_to_axis(from, axisConstraint);
            to = constrain_to_axis(to, axisConstraint);
        }
        
        float4 rotation = make_rotation_quat_between_vectors(from, to);
        float3 axis = axis_from_quat(rotation);
        float angle = angle_from_quat(rotation);
        
        rotation = make_rotation_quat_axis_angle(axis, angle + addition);
        
        currentQuat = normalize(rotation * initialQuat);
    }
    
    void            reset_quat()                                     { currentQuat = initialQuat = float4(0, 0, 0, 1); }
    
    const float4&   get_quat() const                                 { return currentQuat; }
    void            set_quat(const float4 &q)                        { currentQuat = q; }
    
    void            set_sphere(const Sphere &s)                      { arcballSphere = s; }
    const Sphere&   get_sphere() const                               { return arcballSphere; }
    
    void            set_constraint_axis(const float3 &axis)          { axisConstraint = normalize(axis); useConstraints = true; }
    const           float3 & get_constraint_axis() const             { return axisConstraint; }
    
    void            disable_constraints()                            { useConstraints = false; }
    bool            is_using_constraints() const                     { return useConstraints; }
    
    void mouse_on_sphere(const float2 & point, const int2 & windowSize, float3 * resultVector, float * resultAngleAddition)
    {
        float rayT;
        Ray ray = make_ray(*camera, ((float) windowSize.x / (float) windowSize.y), float2(point.x, point.y), float2(windowSize.x, windowSize.y));
        
        // Click inside the sphere?
        if (arcballSphere.intersects(ray, &rayT) > 0)
        {
            // trace a ray through the pixel to the sphere
            *resultVector = normalize(ray.calculate_position(rayT) - arcballSphere.center);
            *resultAngleAddition = 0;
        }
        
        // Not inside the sphere
        else
        {
            // first project the sphere according to the camera, resulting in an ellipse (possible a circle)
            Sphere cameraSpaceSphere(transform_vector(camera->get_view_matrix(), arcballSphere.center), arcballSphere.radius);
            
            float2 center, axisA, axisB;
            
            cameraSpaceSphere.calculate_projection(camera->get_focal_length(), float2(windowSize.x, windowSize.y), &center, &axisA, &axisB);
            
            // find the point closest on the screen-projected ellipse to the mouse
            float2 screenSpaceClosest = math::get_closest_point_on_ellipse(center, axisA, axisB, point);
            
            // and send a ray through that point, finding the closest point on the sphere to it
            Ray newRay = make_ray(*camera, (float) windowSize.x / (float) windowSize.y, screenSpaceClosest, math::float2(windowSize));
            float3 closestPointOnSphere = arcballSphere.closest_point(newRay);
            
            // our result point is the vector between this closest point on the sphere and its center
            *resultVector = normalize(closestPointOnSphere - arcballSphere.center);
            
            // our angle addition is the screen-space distance between the mouse and the closest point on the sphere, divided into
            // the screen-space radius of the sphere's projected ellipse, multiplied by pi
            float screenRadius = std::max(length(axisA), length(axisB));
            *resultAngleAddition = distance(float2(point), screenSpaceClosest) / screenRadius * ANVIL_PI;
        }
    }
    
};

#endif // end arcball_camera_h
