#ifndef arcball_h
#define arcball_h

#include "linear_algebra.hpp"
#include "math_util.hpp"
#include "util.hpp"
#include "geometric.hpp"
#include "geometry.hpp"
#include "camera.hpp"

namespace avl
{

    struct ArcballCamera
    {
        float2 windowSize;
        float2 initialMousePos;
        float4 initialQuat, currentQuat;
        
        ArcballCamera(float2 windowSize) : windowSize(windowSize) { initialQuat = currentQuat = float4(0, 0, 0, 1); }
        
        void mouse_down(const float2 & mousePos)
        {
            initialMousePos = mousePos;
            initialQuat = currentQuat;
        }
        
        void mouse_drag(const float2 & mousePos)
        {
            auto a = mouse_on_sphere(initialMousePos);
            auto b = mouse_on_sphere(mousePos);
            
            auto rotation = make_rotation_quat_between_vectors(a, b);
            currentQuat = safe_normalize(qmul(initialQuat, rotation));
        }
        
        float3 mouse_on_sphere(const float2 & mouse)
        {
            float3 result = {0, 0, 0};
            result.x = (mouse.x - (0.5f * windowSize.x)) / (0.5f * windowSize.x);
            result.y = -(mouse.y - (0.5f * windowSize.y)) / (0.5f * windowSize.y);
            
            float mag = length2(result);
            
            if (mag > 1.0f) result = normalize(result);
            else
            {
                result.z = sqrtf(1.0 - mag);
                result = normalize(result);
            }
            
            return result;
        }
        
    };
    
}

#endif // end arcball_h
