#ifndef camera_h
#define camera_h

#include <vector>
#include <array>
#include "GL_API.hpp"
#include "math_util.hpp"
#include "geometric.hpp"
#include "stb/stb_image_write.h"

namespace avl
{
	inline float4 make_frustum_coords(const float aspectRatio, const float nearClip, const float vfov)
	{
		const float top = nearClip * std::tan((vfov * (float(ANVIL_PI) * 2.f) / 360.0f) / 2.0f);
		const float right = top * aspectRatio;
		const float bottom = -top;
		const float left = -right;
		return{ top, right, bottom, left };
	}

	inline std::array<float3, 4> make_near_clip_coords(Pose pose, float nearClip, float farClip, float aspectRatio, const float vfov)
	{
		const float3 viewDirection = safe_normalize(-pose.zdir());
		const float3 eye = pose.position;

		auto leftDir = pose.xdir();
		auto upDir = pose.ydir();

		const auto coords = make_frustum_coords(aspectRatio, nearClip, vfov);

		float frustumTop = coords[0];
		float frustumRight = coords[1];
		float frustumBottom = coords[2];
		float frustumLeft = coords[3];

		float3 topLeft = eye + (nearClip * viewDirection) + (frustumTop * upDir) + (frustumLeft * leftDir);
		float3 topRight = eye + (nearClip  * viewDirection) + (frustumTop * upDir) + (frustumRight * leftDir);
		float3 bottomLeft = eye + (nearClip  * viewDirection) + (frustumBottom * upDir) + (frustumLeft * leftDir);
		float3 bottomRight = eye + (nearClip  * viewDirection) + (frustumBottom * upDir) + (frustumRight * leftDir);

		return{ topLeft, topRight, bottomLeft, bottomRight };
	}

	inline std::array<float3, 4> make_far_clip_coords(Pose pose, float nearClip, float farClip, float aspectRatio, const float vfov)
	{
		const float3 viewDirection = safe_normalize(-pose.zdir());
		const float3 eye = pose.position;
		const float ratio = farClip / nearClip;

		const auto leftDir = pose.xdir();
		const auto upDir = pose.ydir();

		const auto coords = make_frustum_coords(aspectRatio, nearClip, vfov);

		float frustumTop = coords[0];
		float frustumRight = coords[1];
		float frustumBottom = coords[2];
		float frustumLeft = coords[3];

		float3 topLeft = eye + (farClip * viewDirection) + (ratio * frustumTop * upDir) + (ratio * frustumLeft * leftDir);
		float3 topRight = eye + (farClip * viewDirection) + (ratio * frustumTop * upDir) + (ratio * frustumRight * leftDir);
		float3 bottomLeft = eye + (farClip * viewDirection) + (ratio * frustumBottom * upDir) + (ratio * frustumLeft * leftDir);
		float3 bottomRight = eye + (farClip * viewDirection) + (ratio * frustumBottom * upDir) + (ratio * frustumRight * leftDir);

		return{ topLeft, topRight, bottomLeft, bottomRight };
	}

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
        
        float4x4 get_projection_matrix(float aspectRatio) const
        {
            float4 f = make_frustum_coords(aspectRatio, nearClip, fov);
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
        
        void set_pose(const Pose & p) { pose = p; }

        void set_orientation(const float4 & o) { pose.orientation = safe_normalize(o); }
        
        void set_position(const float3 & pos) { pose.position = pos; }
        
        void set_perspective(float vFov, float nearClip, float farClip)
        {
            this->fov = vFov;
            this->nearClip = nearClip;
            this->farClip = farClip;
        }
        
        void look_at(float3 target)
        {
            pose = look_at_pose_rh(pose.position, target);
        }
        
        void look_at(float3 eyePoint, float3 target)
        {
            pose = look_at_pose_rh(eyePoint, target);
        }
        
        void look_at(float3 eyePoint, float3 target, float3 upDirection)
        {
            pose = look_at_pose_rh(eyePoint, target, upDirection);
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
        
        bool enableSpring = true;
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
            float3 flatLookVec = safe_normalize(float3(lookVec.x, 0, lookVec.z));
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
            
            if (enableSpring)
            {
                float springyX = damped_spring(target.x, current.x, velocity.x, delta, 0.99f);
                float springyY = damped_spring(target.y, current.y, velocity.y, delta, 0.99f);
                float springyZ = damped_spring(target.z, current.z, velocity.z, delta, 0.99f);
                float3 dampedLocation = { springyX, springyY, springyZ };
                cam->set_position(dampedLocation);
            }
            else
            {
                cam->set_position(target);
            }
            
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
        const float top = camera.nearClip * std::tan((camera.fov * (float(ANVIL_PI) / 2.f) / 360.f) / 2.f);
        const float right = top * aspectRatio; // Is this correct?
        const float left = -right;
        float s = (uPos - 0.5f) * imagePlaneApectRatio;
        float t = (vPos - 0.5f);
        float viewDistance = imagePlaneApectRatio / std::abs(right - left) * camera.nearClip;
        return Ray(camera.get_eye_point(), safe_normalize(camera.pose.xdir() * s + camera.pose.ydir() * t - (camera.get_view_direction() * viewDistance)));
    }
    
    inline Ray make_ray(const GlCamera & camera, const float aspectRatio, const float2 & posPixels, const float2 & imageSizePixels)
    {
        return make_ray(camera, aspectRatio, posPixels.x / imageSizePixels.x, (imageSizePixels.y - posPixels.y) / imageSizePixels.y, imageSizePixels.x / imageSizePixels.y);
    }

    class CubemapCamera
    {
        GlFramebuffer framebuffer;
        GlTexture2D colorBuffer;
        GLuint cubeMapHandle;
        float2 resolution;
        std::vector<std::pair<GLenum, Pose>> faces;
        bool shouldCapture = false;

        void save_pngs()
        {
            const std::vector<std::string> faceNames = {{"positive_x"}, {"negative_x"}, {"positive_y"}, {"negative_y"}, {"positive_z"}, {"negative_z"}};
            std::vector<uint8_t> data(resolution.x * resolution.y * 3);
            for (int i = 0; i < 6; ++i)
            {
                glGetTexImage(faces[i].first, 0, GL_RGB, GL_UNSIGNED_BYTE, data.data());
                stbi_write_png(std::string(faceNames[i] + ".png").c_str(), resolution.x, resolution.y, 3, data.data(), resolution.x * 3);
                data.clear();
            }
            shouldCapture = false;
        }

    public:

        std::function<void(float3 eyePosition, float4x4 viewMatrix, float4x4 projMatrix)> render;

        CubemapCamera(float2 resolution) : resolution(resolution)
        {
            this->resolution = resolution;

            colorBuffer.setup(resolution.x, resolution.y, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

            glNamedFramebufferTexture2DEXT(framebuffer, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorBuffer, 0); // attach
            framebuffer.check_complete();
        
            gl_check_error(__FILE__, __LINE__);

            glGenTextures(1, &cubeMapHandle);
            glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMapHandle);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, 0);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        
            for (int i = 0; i < 6; ++i) 
            {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, resolution.x, resolution.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            }
        
            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

            std::vector<float3> targets = {{1, 0, 0,}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
            std::vector<float3> upVecs = {{0, -1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, 1}, {0, -1, 0}, {0, -1, 0}};
            for (int i = 0; i < 6; ++i)
            {
                auto f = std::pair<GLenum, Pose>(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, look_at_pose_rh(float3(0, 0, 0), targets[i], upVecs[i]));
                faces.push_back(f);
            }
     
            gl_check_error(__FILE__, __LINE__);
         }

         GLuint get_cubemap_handle() const { return cubeMapHandle; }
     
         void export_pngs() { shouldCapture = true; }

         void update(float3 eyePosition)
         {
             // Todo: DSA
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
            glViewport(0, 0, resolution.x , resolution.y);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            auto projMatrix = make_perspective_matrix(to_radians(90.f), 1.0f, 0.1f, 128.f); 
            for (int i = 0; i < 6; ++i)
            {
                glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, faces[i].first, cubeMapHandle, 0);
                auto viewMatrix = make_view_matrix_from_pose(faces[i].second);

                if (render) render(eyePosition, viewMatrix, projMatrix);
            }

            if (shouldCapture) save_pngs();

            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        }
    };

}

#endif // camera_h
