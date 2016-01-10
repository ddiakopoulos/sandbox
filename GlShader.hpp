// See COPYING file for attribution information

#ifndef glshader_h
#define glshader_h

#include "linear_algebra.hpp"
#include "math_util.hpp"
#include "geometric.hpp"
#include "util.hpp"
#include "GlShared.hpp"

namespace avl
{
    
    // todo: check uniform exists
    class GlShader : public Noncopyable
    {
        GLuint program;
        bool enabled = false;
        void check() const { if (!enabled) throw std::runtime_error("shader not enabled"); };
        
    public:
        
        GlShader() : program() {}
        GlShader(const std::string & vertexShader, const std::string & fragmentShader, const std::string & geometryShader = "");
        ~GlShader() { if(program) glDeleteProgram(program); }
        
        GlShader(GlShader && r) : GlShader() { *this = std::move(r); }
        
        GLuint get_gl_handle() const { return program; }
        GLint get_uniform_location(const std::string & name) const { return glGetUniformLocation(program, name.c_str()); }
        
        GlShader & operator = (GlShader && r) { std::swap(program, r.program); return *this; }
        
        void uniform(const std::string & name, int scalar) const { check(); glUniform1i(get_uniform_location(name), scalar); }
        void uniform(const std::string & name, float scalar) const { check(); glUniform1f(get_uniform_location(name), scalar); }
        
        void uniform(const std::string & name, const float2 & vec) const { check(); glUniform2fv(get_uniform_location(name), 1, &vec.x); }
        void uniform(const std::string & name, const float3 & vec) const { check(); glUniform3fv(get_uniform_location(name), 1, &vec.x); }
        void uniform(const std::string & name, const float4 & vec) const { check(); glUniform4fv(get_uniform_location(name), 1, &vec.x); }
        
        void uniform(const std::string & name, const mat<float,3,3> & mat) const { check(); glUniformMatrix3fv(get_uniform_location(name), 1, GL_FALSE, &mat.x.x); }
        void uniform(const std::string & name, const mat<float,4,4> & mat) const { check(); glUniformMatrix4fv(get_uniform_location(name), 1, GL_FALSE, &mat.x.x); }
        
        void texture(const std::string & name, int unit, GLuint texId, GLenum textureTarget) const;
        void texture(const char * name, int unit, const GlTexture & tex) const;
        
        void bind() { if (program > 0) enabled = true; glUseProgram(program); }
        void unbind() { enabled = false; glUseProgram(0); }
    };
    
}

#endif // glshader_h
