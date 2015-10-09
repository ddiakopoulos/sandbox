#ifndef gl_common_h
#define gl_common_h

#include <vector>
#include <type_traits>

#if defined(ANVIL_PLATFORM_WINDOWS)
    #define GLEW_STATIC
    #include <GL/glew.h>
#elif defined(ANVIL_PLATFORM_OSX)
    #include <OpenGL/gl3.h>
#endif

namespace gfx
{
    
    template<typename T>
    inline GLenum to_gl(T *)
    {
        if (std::is_same<T, int8_t *>::value) return GL_UNSIGNED_BYTE;
        else if (std::is_same<T, uint16_t *>::value) return GL_UNSIGNED_SHORT;
        else if (std::is_same<T, uint32_t *>::value) return GL_UNSIGNED_INT;
        else if (std::is_same<T, float *>::value) return GL_FLOAT;
    };
    
    // Can be used for things like a vbo, ibo, or pbo
    class GlBuffer : public util::Noncopyable
    {
        GLuint buffer;
        GLsizeiptr bufferLen;
    public:
        
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
        
        float fov = 45.0f;
        float nearClip = 0.1f;
        float farClip = 128.0f;
        
        math::Pose get_pose() const { return pose; }
        
        math::float3 get_view_direction() const { return -pose.zdir(); }
        
        math::float3 get_eye_point() const { return pose.position; }
        
        math::float4x4 get_view_matrix() const { return math::make_view_matrix_from_pose(pose); }
        
        math::float4x4 get_projection_matrix(float aspectRatio) const
        {
            const float top = nearClip * std::tan((fov * (ANVIL_PI / 2) / 360) / 2);
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
            const math::float3 worldUp = {0,1,0};
            pose.position = eyePoint;
            math::float3 zDir = math::normalize(eyePoint - target);
            math::float3 xDir = math::normalize(cross(worldUp, zDir));
            math::float3 yDir = math::cross(zDir, xDir);
            pose.orientation = math::normalize(math::make_rotation_quat_from_rotation_matrix({xDir, yDir, zDir}));
        }
    };
    
    struct GlTexture
    {
        
    };
    
    struct GlFramebuffer
    {
        
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
