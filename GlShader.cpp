#include "GlShader.hpp"

using namespace gfx;
using namespace math;

#include <map>
#include <vector>

static void compile_shader(GLuint program, GLenum type, const char * source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    
    GLint status;
    GLint length;
    
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    
    if (status == GL_FALSE)
    {
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::vector<GLchar> buffer(length);
        glGetShaderInfoLog(shader, (GLsizei) buffer.size(), nullptr, buffer.data());
        glDeleteShader(shader);
        std::cerr << "GL Compile Error: " << buffer.data() << std::endl;
        std::cerr << "Source: " << source << std::endl;
        throw std::runtime_error("GLSL Compile Failure");
    }
    
    glAttachShader(program, shader);
    glDeleteShader(shader);
}

GlShader::GlShader(const std::string & vertexShader, const std::string & fragmentShader, const std::string & geometryShader) : GlShader()
{
    program = glCreateProgram();
    compile_shader(program, GL_VERTEX_SHADER, vertexShader.c_str());
    compile_shader(program, GL_FRAGMENT_SHADER, fragmentShader.c_str());
    
    if (geometryShader.length() != 0)
        compile_shader(program, GL_GEOMETRY_SHADER, geometryShader.c_str());
    
    glLinkProgram(program);
    
    GLint status;
    GLint length;
    
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    
    if (status == GL_FALSE)
    {
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::vector<GLchar> buffer(length);
        glGetProgramInfoLog(program, (GLsizei) buffer.size(), nullptr, buffer.data());
        std::cerr << "GL Link Error: " << buffer.data() << std::endl;
        throw std::runtime_error("GLSL Link Failure");
    }
}

void GlShader::texture(const std::string & name, int unit, GLuint texId, GLenum textureTarget) const
{
    check();
    glUniform1i(get_uniform_location(name), unit);
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(textureTarget, texId);
}
