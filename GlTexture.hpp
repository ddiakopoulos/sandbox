#ifndef gltexture_h
#define gltexture_h

#include "linear_algebra.hpp"
#include "math_util.hpp"
#include "geometric.hpp"
#include "geometry.hpp"
#include "util.hpp"
#include "GlMesh.hpp"
#include "GlShader.hpp"
#include "GlShared.hpp"

namespace gfx
{
    static const char s_textureVert[] = R"(#version 330
        layout(location = 0) in vec3 position;
        layout(location = 3) in vec2 uvs;
        uniform mat4 u_model;
        uniform mat4 u_projection;
        out vec2 texCoord;
        void main()
        {
            texCoord = uvs;
            gl_Position = u_projection * u_model * vec4(position.xy, 0.0, 1.0);

        }
    )";
    
    // texture (core profile on OSX) vs texture2D
    static const char s_textureFrag[] = R"(#version 330
        uniform sampler2D u_texture;
        in vec2 texCoord;
        out vec4 f_color;
        void main()
        {
            f_color = texture(u_texture, texCoord);
        }
    )";
    
    struct GLTextureView : public util::Noncopyable
    {
        GlShader program;
        util::Model mesh;
        GLuint texture;
        
        GLTextureView(GLuint tex) : texture(tex)
        {
            util::Geometry g;
            g.vertices = { {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f} };
            g.texCoords = { {1.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f} };
            g.faces = {{0, 1, 2}, {3, 4, 5}};
            mesh = make_model_from_geometry(g);
            program = GlShader(s_textureVert, s_textureFrag);
        }
        
        void draw(int x, int y, int width, int height)
        {
            program.bind();
            
            const math::float4x4 projection = math::make_orthographic_perspective_matrix(0.0f, 300.f, 300.f, 0.0f, -1.0f, 1.0f);
            
            math::float4x4 model = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
            model = math::mul(math::make_scaling_matrix({ (float) width, (float) height, 0.f}), model);
            model = math::mul(math::make_translation_matrix({(float)x, (float)y, 0.f}), model);
            model = math::mul(math::make_translation_matrix({0.5f * width, 0.5f * height, 0.0f}), model);
            model = math::mul(math::make_translation_matrix({-0.5f * width, -0.5f * height, 0.0f}), model);

            program.uniform("u_model", model);
            program.uniform("u_projection", projection);
            program.texture("u_texture", 0, texture, GL_TEXTURE_2D);
            
            mesh.draw();
            
            program.unbind();
        }
        
    };
    
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
            
            glTexImage2D(GL_TEXTURE_2D, 0,internalFormat, width, height, 0, externalFormat, type, pixels);
            
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
    
} // end namespace gfx

#endif // gltexture_h
