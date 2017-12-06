#ifndef camera_h
#define camera_h

#include "gl-api.hpp"
#include "math-core.hpp"

#include "stb/stb_image_write.h"

namespace avl
{

    ////////////////////////////////////////////////
    //   Basic Retained-Mode Perspective Camera   //
    ////////////////////////////////////////////////

    class GlCamera
    {
        Pose pose;

    public:

        float vfov{ 1.3f };
        float nearclip{ 0.01f };
        float farclip{ 64.f };

        float4x4 get_view_matrix() const { return pose.view_matrix(); }
        float4x4 get_projection_matrix(float aspectRatio) const { return make_projection_matrix(vfov, aspectRatio, nearclip, farclip); }

        Pose get_pose() const { return pose; }
        Pose & get_pose() { return pose; }

        void set_pose(const Pose & p) { pose = p; }

        float3 get_view_direction() const { return -pose.zdir(); }
        float3 get_eye_point() const { return pose.position; }

        void look_at(const float3 & target) { pose = look_at_pose_rh(pose.position, target); }
        void look_at(const float3 & eyePoint, const float3 target) { pose = look_at_pose_rh(eyePoint, target); }
        void look_at(const float3 & eyePoint, float3 const & target, float3 const & worldup) { pose = look_at_pose_rh(eyePoint, target, worldup); }
        
        Ray get_world_ray(const float2 cursor, const float2 viewport)
        {
            const float aspect = viewport.x / viewport.y;
            auto cameraRay = ray_from_viewport_pixel(cursor, viewport, get_projection_matrix(aspect));
            return pose * cameraRay;
        }
    };
    
    /////////////////////////////////////
    //   Standard Free-Flying Camera   //
    /////////////////////////////////////

    class FlyCameraController
    {
        GlCamera * cam;
        
        float camPitch = 0, camYaw = 0;
        float4 orientation, lastOrientation;
        
        bool bf = 0, bl = 0, bb = 0, br = 0, ml = 0, mr = 0;
        float2 lastCursor;
        
    public:
        
        bool enableSpring = true;
        float movementSpeed = 14.0f;
        float3 velocity;

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
                    case GLFW_KEY_W: bf = e.is_down(); break;
                    case GLFW_KEY_A: bl = e.is_down(); break;
                    case GLFW_KEY_S: bb = e.is_down(); break;
                    case GLFW_KEY_D: br = e.is_down(); break;
                }
                break;
            case InputEvent::MOUSE:
                switch (e.value[0])
                {
                    case GLFW_MOUSE_BUTTON_LEFT: ml = e.is_down(); break;
                    case GLFW_MOUSE_BUTTON_RIGHT: mr = e.is_down(); break;
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

        void update(float delta)
        {
            float3 move;
            
            float instantaneousSpeed = movementSpeed;

            if (bf || (ml && mr))
            {
                move.z -= 1 * instantaneousSpeed;
                instantaneousSpeed *= 0.75f;
            }
            if (bl)
            {
                move.x -= 1 * instantaneousSpeed;
                instantaneousSpeed *= 0.75f;
            }
            if (bb)
            {
                move.z += 1 * instantaneousSpeed;
                instantaneousSpeed *= 0.75f;
            }
            if (br)
            {
                move.x += 1 * instantaneousSpeed;
                instantaneousSpeed *= 0.75f;
            }
            
            float3 & current = cam->get_pose().position;
            const float3 target = cam->get_pose().transform_coord(move);
            
            if (enableSpring)
            {
                critically_damped_spring(delta, target.x, 1.f, instantaneousSpeed, current.x, velocity.x);
                critically_damped_spring(delta, target.y, 1.f, instantaneousSpeed, current.y, velocity.y);
                critically_damped_spring(delta, target.z, 1.f, instantaneousSpeed, current.z, velocity.z);
            }
            else
            {
                Pose & camPose = cam->get_pose();
                camPose.position = target;
            }
            
            float3 lookVec;
            lookVec.x = cam->get_eye_point().x - 1.f * cosf(camPitch) * sinf(camYaw);
            lookVec.y = cam->get_eye_point().y + 1.f * sinf(camPitch);
            lookVec.z = cam->get_eye_point().z - 1.f * cosf(camPitch) * cosf(camYaw);
            cam->look_at(lookVec);
        }
    };
    
    ////////////////////////
    //   Cubemap Camera   //
    ////////////////////////

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
            std::vector<uint8_t> data(static_cast<int>(resolution.x) * static_cast<int>(resolution.y) * 3);
            for (int i = 0; i < 6; ++i)
            {
                glGetTexImage(faces[i].first, 0, GL_RGB, GL_UNSIGNED_BYTE, data.data());
                stbi_write_png(
                    std::string(faceNames[i] + ".png").c_str(), 
                    static_cast<int>(resolution.x), 
                    static_cast<int>(resolution.y), 
                    3, 
                    data.data(), 
                    static_cast<int>(resolution.x) * 3
                );
                data.clear();
            }
            shouldCapture = false;
        }

    public:

        std::function<void(float3 eyePosition, float4x4 viewMatrix, float4x4 projMatrix)> render;

        CubemapCamera(float2 resolution) : resolution(resolution)
        {
            this->resolution = resolution;

            colorBuffer.setup(static_cast<int>(resolution.x), static_cast<int>(resolution.y), GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

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
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, static_cast<int>(resolution.x), static_cast<int>(resolution.y), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
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
            glViewport(0, 0, static_cast<int>(resolution.x) , static_cast<int>(resolution.y));
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            auto projMatrix = make_projection_matrix(to_radians(90.f), 1.0f, 0.1f, 128.f); 
            for (int i = 0; i < 6; ++i)
            {
                glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, faces[i].first, cubeMapHandle, 0);
                auto viewMatrix = faces[i].second.view_matrix();

                if (render) render(eyePosition, viewMatrix, projMatrix);
            }

            if (shouldCapture) save_pngs();

            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        }
    };

}

#endif // camera_h
