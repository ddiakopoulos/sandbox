#ifndef gltexture_h
#define gltexture_h

#include "linear_algebra.hpp"
#include "math_util.hpp"
#include "geometric.hpp"
#include "util.hpp"
#include "GlMesh.hpp"
#include "GlShader.hpp"
#include "GlShared.hpp"
#include "file_io.hpp"

#include "third_party/stb_image.h"

namespace gfx
{

    
    class GlTexture : public util::Noncopyable
    {
        math::int2 size;
        GLuint internalFormat;
        GLuint handle;
        
    public:
        
        GlTexture() : handle() {}
        GlTexture(int w, int h, GLuint id) : size(w, h), handle(id) {}
        GlTexture(GlTexture && r) : GlTexture() { *this = std::move(r); }
        ~GlTexture() { if (handle) glDeleteTextures(1, &handle); }
        GlTexture & operator = (GlTexture && r) { std::swap(handle, r.handle); std::swap(size, r.size); return *this; }
        
        GLuint get_gl_handle() const { return handle; }
        
        math::int2 get_size() const { return size; }
        
        void image2D(GLenum target, GLint level, GLenum internal_fmt, const math::int2 & size, GLenum format, GLenum type, const GLvoid * pixels)
        {
            if (!handle)
                glGenTextures(1, &handle);
            
            glBindTexture(target, handle);
            glTexImage2D(target, level, internal_fmt, size.x, size.y, 0, format, type, pixels);
            glBindTexture(target, 0);
            
            this->size = size;
            this->internalFormat = internal_fmt;
        }
        
        void allocate(GLsizei width, GLsizei height, GLenum format)
        {
            if (format == GL_DEPTH_COMPONENT)
            {
                internalFormat = GL_DEPTH_COMPONENT;
                load_data(width, height, format, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
                return;
            }
            internalFormat = format;
            load_data(width, height, format, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        }
        
        void load_data(GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid * pixels, bool createMipmap = false)
        {
            if (!handle)
                glGenTextures(1, &handle);
            
            glBindTexture(GL_TEXTURE_2D, handle);
            
            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, type, pixels);
            
            if (createMipmap)
                glGenerateMipmap(GL_TEXTURE_2D);
            
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, createMipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, 0);
            
            size = {width, height};
        }
        
        void load_data(GLsizei width, GLsizei height, GLenum internalFormat, GLenum externalFormat, GLenum type, const GLvoid * pixels)
        {
            if (!handle)
                glGenTextures(1, &handle);
            
            glBindTexture(GL_TEXTURE_2D, handle);
            
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, externalFormat, type, pixels);
            
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            
            glBindTexture(GL_TEXTURE_2D, 0);
            
            size = {width, height};
        }
        
        void parameter(GLenum name, GLint param)
        {
            if (!handle) 
                glGenTextures(1, &handle);
            
            glBindTexture(GL_TEXTURE_2D, handle);
            glTexParameteri(GL_TEXTURE_2D, name, param);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        
    };
    
    inline GlTexture load_image(const std::string & path)
    {
        auto binaryFile = util::read_file_binary(path);
        
        int width, height, nBytes;
        auto data = stbi_load_from_memory(binaryFile.data(), (int) binaryFile.size(), &width, &height, &nBytes, 0);
        
        GlTexture tex;
        switch(nBytes)
        {
            case 3: tex.load_data(width, height, GL_RGB, GL_UNSIGNED_BYTE, data, true); break;
            case 4: tex.load_data(width, height, GL_RGBA, GL_UNSIGNED_BYTE, data, true); break;
        }
        
        stbi_image_free(data);
        return tex;
    }
    
} // end namespace gfx

#endif // gltexture_h
