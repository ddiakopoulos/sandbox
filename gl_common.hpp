#ifndef gl_common_h
#define gl_common_h

#include <vector>

#if defined(ANVIL_PLATFORM_WINDOWS)
    #define GLEW_STATIC
    #include <GL/glew.h>
#elif defined(ANVIL_PLATFORM_OSX)
    #include <OpenGL/gl3.h>
#endif

namespace gfx
{
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
        
    };
    
    struct GlTexture
    {
        
    };
    
    struct GlFramebuffer
    {
        
    };
    
    inline GLenum GetType(uint8_t *) { return GL_UNSIGNED_BYTE; }
    inline GLenum GetType(uint16_t *) { return GL_UNSIGNED_SHORT; }
    inline GLenum GetType(uint32_t *) { return GL_UNSIGNED_INT; }
    inline GLenum GetType(float *) { return GL_FLOAT; }

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
