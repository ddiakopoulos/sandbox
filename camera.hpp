#ifndef camera_h
#define camera_h

#include <vector>
#include "GL_API.hpp"
#include "math_util.hpp"
#include "geometric.hpp"

namespace avl
{
    
    struct GlCamera
    {
        Pose pose;
        
        float fov = 60.0f;
        float nearClip = 0.1f;
        float farClip = 70.0f;
        
        Pose get_pose() const { return pose; }
        
        float3 get_view_direction() const { return -pose.zdir(); }
        
        float3 get_eye_point() const { return pose.position; }
        
        float4x4 get_view_matrix() const { return make_view_matrix_from_pose(pose); }
        
        float4 make_frustum_coords(float aspectRatio) const
        {
            const float top = nearClip * std::tan((fov * (ANVIL_PI * 2.f) / 360.0f) / 2.0f);
            const float right = top * aspectRatio;
            const float bottom = -top;
            const float left = -right;
            return {top, right, bottom, left};
        }
        
        float4x4 get_projection_matrix(float aspectRatio) const
        {
            float4 f = make_frustum_coords(aspectRatio);
            return make_projection_matrix(f[3], f[1], f[2], f[0], nearClip, farClip); // fixme
        }
        
        float4x4 get_projection_matrix(float l, float r, float b, float t) const
        {
            float left = -tanf(to_radians(l)) * nearClip;
            float right = tanf(to_radians(r)) * nearClip;
            float bottom = -tanf(to_radians(b)) * nearClip;
            float top = tanf(to_radians(t)) * nearClip;
            return make_projection_matrix(left, right, bottom, top, nearClip, farClip);
        }
        
        void set_orientation(float4 o) { pose.orientation = normalize(o); }
        
        void set_position(float3 p) { pose.position = p; }
        
        void set_perspective(float vFov, float nearClip, float farClip)
        {
            this->fov = vFov;
            this->nearClip = nearClip;
            this->farClip = farClip;
        }
        
        void look_at(float3 target) { look_at(pose.position, target); }
        
        void look_at(float3 eyePoint, float3 target)
        {
            look_at_pose(eyePoint, target, pose);
        }
        
        float get_focal_length() const
        {
            return (1.f / (tan(to_radians(fov) * 0.5f) * 2.0f));
        }
        
        Ray get_world_ray(float2 cursor, float2 viewport)
        {
            float aspect = viewport.x / viewport.y;
            auto cameraRay = ray_from_viewport_pixel(cursor, viewport, get_projection_matrix(aspect));
            return pose * cameraRay;
        }
        
    };
    
    class FlyCameraController
    {
        GlCamera * cam;
        
        float camPitch = 0, camYaw = 0;
        float4 orientation, lastOrientation;
        
        bool bf = 0, bl = 0, bb = 0, br = 0, ml = 0, mr = 0;
        float2 lastCursor;
        
    public:
        
        float movementSpeed = 21.00f;
        
        float3 lastLook;
        
        FlyCameraController() {}
        
        FlyCameraController(GlCamera * cam) : cam(cam)
        {
            update_yaw_pitch();
        }
        
        void set_camera(GlCamera * cam)
        {
            this->cam = cam;
            update_yaw_pitch();
        }
        
        void update_yaw_pitch()
        {
            const float3 worldNorth = {0, 0, -1};
            float3 lookVec = cam->get_view_direction();
            float3 flatLookVec = normalize(float3(lookVec.x, 0, lookVec.z));
            camYaw = std::acos(clamp(dot(worldNorth, flatLookVec), -1.0f, +1.0f)) * (flatLookVec.x > 0 ? -1 : 1);
            camPitch = std::acos(clamp(dot(lookVec, flatLookVec), -1.0f, +1.0f)) * (lookVec.y > 0 ? 1 : -1);
        }
        
        void handle_input(const InputEvent & e)
        {
            switch (e.type)
            {
                case InputEvent::KEY:
                    switch (e.value[0])
                {
                    case GLFW_KEY_W: bf = e.is_mouse_down(); break;
                    case GLFW_KEY_A: bl = e.is_mouse_down(); break;
                    case GLFW_KEY_S: bb = e.is_mouse_down(); break;
                    case GLFW_KEY_D: br = e.is_mouse_down(); break;
                }
                    break;
                case InputEvent::MOUSE:
                    switch (e.value[0])
                {
                    case GLFW_MOUSE_BUTTON_LEFT: ml = e.is_mouse_down(); break;
                    case GLFW_MOUSE_BUTTON_RIGHT: mr = e.is_mouse_down(); break;
                }
                    break;
                case InputEvent::CURSOR:
                    if (mr)
                    {
                        camYaw -= (e.cursor.x - lastCursor.x) * 0.01f;
                        camPitch = clamp(camPitch - (e.cursor.y - lastCursor.y) * 0.01f, -1.57f, +1.57f);
                    }
                    break;
            }
            lastCursor = e.cursor;
        }
        
        float3 velocity = float3(0, 0, 0);
        
        void update(float delta)
        {
            float3 move;
            
            if (bf || (ml && mr)) move.z -= 1 * movementSpeed;
            
            if (bl)
                move.x -= 1 * movementSpeed;
            if (bb)
                move.z += 1 * movementSpeed;
            if (br)
                move.x += 1 * movementSpeed;
            
            auto current = cam->get_pose().position;
            auto target = cam->get_pose().transform_coord(move);
            
            float springyX = damped_spring(target.x, current.x, velocity.x, delta, 0.99);
            float springyY = damped_spring(target.y, current.y, velocity.y, delta, 0.99);
            float springyZ = damped_spring(target.z, current.z, velocity.z, delta, 0.99);
            
            float3 dampedLocation = {springyX, springyY, springyZ};
            cam->set_position(dampedLocation);
            
            float3 lookVec;
            lookVec.x = cam->get_eye_point().x - 1.f * cosf(camPitch) * sinf(camYaw);
            lookVec.y = cam->get_eye_point().y + 1.f * sinf(camPitch);
            lookVec.z = cam->get_eye_point().z + -1.f * cosf(camPitch) * cosf(camYaw);
            lastLook = lookVec;
            cam->look_at(lookVec);
        }
    };
    
    inline Ray make_ray(const GlCamera & camera, const float aspectRatio, float uPos, float vPos, float imagePlaneApectRatio)
    {
        const float top = camera.nearClip * std::tan((camera.fov * (ANVIL_PI / 2) / 360) / 2);
        const float right = top * aspectRatio; // Is this correct?
        const float left = -right;
        float s = (uPos - 0.5f) * imagePlaneApectRatio;
        float t = (vPos - 0.5f);
        float viewDistance = imagePlaneApectRatio / std::abs(right - left) * camera.nearClip;
        return Ray(camera.get_eye_point(), normalize(camera.pose.xdir() * s + camera.pose.ydir() * t - (camera.get_view_direction() * viewDistance)));
    }
    
    inline Ray make_ray(const GlCamera & camera, const float aspectRatio, const float2 & posPixels, const float2 & imageSizePixels)
    {
        return make_ray(camera, aspectRatio, posPixels.x / imageSizePixels.x, (imageSizePixels.y - posPixels.y) / imageSizePixels.y, imageSizePixels.x / imageSizePixels.y);
    }

}

#endif // camera_h
