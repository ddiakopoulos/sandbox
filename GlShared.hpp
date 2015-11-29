#pragma once

#ifndef gl_common_h
#define gl_common_h

#include <vector>
#include <type_traits>
#include "glfw_app.hpp"

#if defined(ANVIL_PLATFORM_WINDOWS)

#elif defined(ANVIL_PLATFORM_OSX)
    #include <OpenGL/gl3.h>
#endif

namespace gfx
{
    
    template<typename T>
    inline GLenum to_gl(T *)
    {
        if      (std::is_same<T, int8_t *>::value)      return GL_UNSIGNED_BYTE;
        else if (std::is_same<T, uint16_t *>::value)    return GL_UNSIGNED_SHORT;
        else if (std::is_same<T, uint32_t *>::value)    return GL_UNSIGNED_INT;
        else if (std::is_same<T, float *>::value)       return GL_FLOAT;
    };
    
    class Ray
    {
        bool signX, signY, signZ;
        math::float3 invDirection;
    public:
        
        math::float3 origin;
        math::float3 direction;
        
        Ray() {}
        Ray(const math::float3 &aOrigin, const math::float3 &aDirection) : origin(aOrigin) { set_direction(aDirection); }
        
        void set_origin(const math::float3 &aOrigin) { origin = aOrigin; }
        const math::float3& get_origin() const { return origin; }
        
        void set_direction(const math::float3 &aDirection)
        {
            direction = aDirection;
            invDirection = math::float3(1.0f / direction.x, 1.0f / direction.y, 1.0f / direction.z);
            signX = (direction.x < 0.0f) ? 1 : 0;
            signY = (direction.y < 0.0f) ? 1 : 0;
            signZ = (direction.z < 0.0f) ? 1 : 0;
        }

        const math::float3 & get_direction() const { return direction; }
        const math::float3 & get_inv_direction() const { return invDirection; }
        
        char getSignX() const { return signX; }
        char getSignY() const { return signY; }
        char getSignZ() const { return signZ; }
        
        void transform(const math::float4x4 & matrix)
        {
            origin = transform_vector(matrix, origin);
            set_direction(math::get_rotation_submatrix(matrix) * direction);
        }
        
        Ray transformed(const math::float4x4 & matrix) const
        {
            Ray result;
            result.origin = transform_vector(matrix, origin);
            result.set_direction(math::get_rotation_submatrix(matrix) * direction);
            return result;
        }
        
        math::float3 calculate_position(float t) const { return origin + direction * t; }
    };
    
    inline Ray operator * (const math::Pose & pose, const Ray & ray) { return {pose.transform_coord(ray.get_origin()), pose.transform_vector(ray.get_direction())}; }
    
    inline Ray between(const math::float3 & start, const math::float3 & end) { return {start, normalize(end - start)}; }
    
    inline Ray ray_from_viewport_pixel(const math::float2 & pixelCoord, const math::float2 & viewportSize, const math::float4x4 & projectionMatrix)
    {
        float vx = pixelCoord.x * 2 / viewportSize.x - 1, vy = 1 - pixelCoord.y * 2 / viewportSize.y;
        auto invProj = inv(projectionMatrix);
        return {{0,0,0}, normalize(transform_coord(invProj, {vx, vy, +1}) - transform_coord(invProj, {vx, vy, -1}))};
    }
    
    // Can be used for things like a vbo, ibo, or pbo
    class GlBuffer : public util::Noncopyable
    {
        GLuint buffer;
        GLsizeiptr bufferLen;
    public:
        
        enum class Type : int
        {
            Vertex,
            Index,
            Pixel,
            Uniform
        };
        
        enum class Usage : int
        {
            Static,
            Dynamic
        };
        
        GlBuffer() : buffer() {}
        GlBuffer(GlBuffer && r) : GlBuffer() { *this = std::move(r); }
        
        ~GlBuffer() { if (buffer) glDeleteBuffers(1, &buffer); }
        
        GLuint gl_handle() const { return buffer; }
        GLsizeiptr size() const { return bufferLen; }
        
        void bind(GLenum target) const { glBindBuffer(target, buffer); }
        void unbind(GLenum target) const { glBindBuffer(target, 0); }
        
        GlBuffer & operator = (GlBuffer && r) { std::swap(buffer, r.buffer); std::swap(bufferLen, r.bufferLen); return *this; }
        
        void set_buffer_data(GLenum target, GLsizeiptr length, const GLvoid * data, GLenum usage)
        {
            if (!buffer) glGenBuffers(1, &buffer);
            glBindBuffer(target, buffer);
            glBufferData(target, length, data, usage);
            glBindBuffer(target, 0);
            this->bufferLen = length;
        }
        
        void set_buffer_data(GLenum target, const std::vector<GLubyte> & bytes, GLenum usage)
        {
            set_buffer_data(target, bytes.size(), bytes.data(), usage);
        }
    };
    
    struct GlCamera
    {
        math::Pose pose;
        
        float fov = 60.0f;
        float nearClip = 0.1f;
        float farClip = 70.0f;
        
        math::Pose get_pose() const { return pose; }
        
        math::float3 get_view_direction() const { return -pose.zdir(); }
        
        math::float3 get_eye_point() const { return pose.position; }
        
        math::float4x4 get_view_matrix() const { return math::make_view_matrix_from_pose(pose); }
        
        math::float4x4 get_projection_matrix(float aspectRatio) const
        {
            const float top = nearClip * std::tan((fov * (ANVIL_PI * 2.f) / 360.0f) / 2.0f);
            const float right = top * aspectRatio;
            const float bottom = -top;
            const float left = -right;
            return math::make_projection_matrix_from_frustrum_rh_gl(left, right, bottom, top, nearClip, farClip);
        }
        
        math::float4x4 get_projection_matrix(float l, float r, float b, float t) const
        {
            float left = -tanf(math::to_radians(l)) * nearClip;
            float right = tanf(math::to_radians(r)) * nearClip;
            float bottom = -tanf(math::to_radians(b)) * nearClip;
            float top = tanf(math::to_radians(t)) * nearClip;
            return math::make_projection_matrix_from_frustrum_rh_gl(left, right, bottom, top, nearClip, farClip);
        }
        
        void set_orientation(math::float4 o) { pose.orientation = math::normalize(o); }
        
        void set_position(math::float3 p) { pose.position = p; }
        
        void set_perspective(float vFov, float nearClip, float farClip)
        {
            this->fov = vFov;
            this->nearClip = nearClip;
            this->farClip = farClip;
        }
        
        void look_at(math::float3 target) { look_at(pose.position, target); }
        
        void look_at(math::float3 eyePoint, math::float3 target)
        {
            look_at_pose(eyePoint, target, pose);
        }
        
        float get_focal_length() const
        {
            return (1.f / (tan(math::to_radians(fov) * 0.5f) * 2.0f));
        }
        
        Ray get_world_ray(math::float2 cursor, math::float2 viewport)
        {
            float aspect = viewport.x / viewport.y;
            auto cameraRay = ray_from_viewport_pixel(cursor, viewport, get_projection_matrix(aspect));
            return pose * cameraRay;
        }

    };
    
    class FPSCameraController
    {
        GlCamera * cam;
        
        float camPitch = 0, camYaw = 0;
        math:: float4 orientation, lastOrientation;
        
        bool bf = 0, bl = 0, bb = 0, br = 0, ml = 0, mr = 0;
        math::float2 lastCursor;
        
    public:
        
        float movementSpeed = 0.25f;
        
        math::float3 lastLook;
        
        FPSCameraController()
        {
            
        }
        
        FPSCameraController(GlCamera * cam) : cam(cam)
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
            const math::float3 worldNorth = {0, 0, -1};
            math::float3 lookVec = cam->get_view_direction();
            math::float3 flatLookVec = normalize(math::float3(lookVec.x, 0, lookVec.z));
            camYaw = std::acos(math::clamp(dot(worldNorth, flatLookVec), -1.0f, +1.0f)) * (flatLookVec.x > 0 ? -1 : 1);
            camPitch = std::acos(math::clamp(dot(lookVec, flatLookVec), -1.0f, +1.0f)) * (lookVec.y > 0 ? 1 : -1);
        }
        
        void handle_input(const util::InputEvent & e)
        {
            switch (e.type)
            {
                case util::InputEvent::KEY:
                    switch (e.value[0])
                    {
                        case GLFW_KEY_W: bf = e.is_mouse_down(); break;
                        case GLFW_KEY_A: bl = e.is_mouse_down(); break;
                        case GLFW_KEY_S: bb = e.is_mouse_down(); break;
                        case GLFW_KEY_D: br = e.is_mouse_down(); break;
                    }
                    break;
                case util::InputEvent::MOUSE:
                    switch (e.value[0])
                    {
                        case GLFW_MOUSE_BUTTON_LEFT: ml = e.is_mouse_down(); break;
                        case GLFW_MOUSE_BUTTON_RIGHT: mr = e.is_mouse_down(); break;
                    }
                    break;
                case util::InputEvent::CURSOR:
                    if (mr)
                    {
                        camYaw -= (e.cursor.x - lastCursor.x) * 0.01f;
                        camPitch = math::clamp(camPitch - (e.cursor.y - lastCursor.y) * 0.01f, -1.57f, +1.57f);
                    }
                    break;
            }
            lastCursor = e.cursor;
        }
        
        math::float3 velocity = math::float3(0, 0, 0);
        
        void update(float delta)
        {
            math::float3 move;
            
            if (bf || (ml && mr)) move.z -= 1 * movementSpeed;
            
            if (bl)
                move.x -= 1 * movementSpeed;
            if (bb)
                move.z += 1 * movementSpeed;
            if (br)
                move.x += 1 * movementSpeed;
            
            auto current = cam->get_pose().position;
            auto target = cam->get_pose().transform_coord(move);
            
            float springyX = math::damped_spring(target.x, current.x, velocity.x, delta, 0.99);
            float springyY = math::damped_spring(target.y, current.y, velocity.y, delta, 0.99);
            float springyZ = math::damped_spring(target.z, current.z, velocity.z, delta, 0.99);
            
            math::float3 dampedLocation = {springyX, springyY, springyZ};
            cam->set_position(target);

            math::float3 lookVec;
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
    
    inline Ray make_ray(const GlCamera & camera, const float aspectRatio, const math::float2 & posPixels, const math::float2 & imageSizePixels)
    {
        return make_ray(camera, aspectRatio, posPixels.x / imageSizePixels.x, (imageSizePixels.y - posPixels.y) / imageSizePixels.y, imageSizePixels.x / imageSizePixels.y);
    }
    
    class GlRenderbuffer : public util::Noncopyable
    {
        GLuint renderbuffer;
        math::int2 size;
    public:
        
        GlRenderbuffer() : renderbuffer() {}
        GlRenderbuffer(GLenum internalformat, GLsizei width, GLsizei height)
        {
            glGenRenderbuffers(1, &renderbuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
            glRenderbufferStorage(GL_RENDERBUFFER, internalformat, width, height);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
            size = {width, height};
        }
        
        GlRenderbuffer(GlRenderbuffer && r) : GlRenderbuffer() { *this = std::move(r); }
        ~GlRenderbuffer() { if(renderbuffer) glDeleteRenderbuffers(1, &renderbuffer); }
        
        GlRenderbuffer & operator = (GlRenderbuffer && r) { std::swap(renderbuffer, r.renderbuffer); std::swap(size, r.size); return *this; }
        
        GLuint get_handle() const { return renderbuffer; }
        math::int2 get_size() const { return size; }
    };
    
    struct GlTexture;
    class GlFramebuffer : public util::Noncopyable
    {
        GLuint handle;
        math::float2 size;
    public:
        GlFramebuffer() : handle() {}
        GlFramebuffer(GlFramebuffer && r) : GlFramebuffer() { *this = std::move(r); }
        ~GlFramebuffer() { if(handle) glDeleteFramebuffers(1, &handle); }
        GlFramebuffer & operator = (GlFramebuffer && r) { std::swap(handle, r.handle); std::swap(size, r.size); return *this; }
        
        GLuint get_handle() const { return handle; }
        
        bool check_complete() const
        {
            glBindFramebuffer(GL_FRAMEBUFFER, handle);
            auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return status == GL_FRAMEBUFFER_COMPLETE;
        }
        
        void attach(GLenum attachment, const GlTexture & tex);
        void attach(GLenum attachment, const GlRenderbuffer & rb);
        
        void bind_to_draw()
        {
            glBindFramebuffer(GL_FRAMEBUFFER, handle);
            glViewport(0, 0, size.x, size.y);
        }
        
        void unbind()
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
    };

    inline void gl_check_error(const char * file, int32_t line)
    {
        GLint error = glGetError();
        if (error)
        {
            const char * errorStr = 0;
            switch (error)
            {
                case GL_INVALID_ENUM: errorStr = "GL_INVALID_ENUM"; break;
                case GL_INVALID_VALUE: errorStr = "GL_INVALID_VALUE"; break;
                case GL_INVALID_OPERATION: errorStr = "GL_INVALID_OPERATION"; break;
                case GL_OUT_OF_MEMORY: errorStr = "GL_OUT_OF_MEMORY"; break;
                default: errorStr = "unknown error"; break;
            }
            printf("GL error : %s, line %d : %s\n", file, line, errorStr);
            error = 0;
        }
    }
}

#endif // end gl_common_h
