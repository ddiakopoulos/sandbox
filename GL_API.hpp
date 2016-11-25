#pragma once

#ifndef AVL_GL_API_H
#define AVL_GL_API_H

#include <vector>
#include <type_traits>
#include "glfw_app.hpp"
#include "file_io.hpp"

#if defined(ANVIL_PLATFORM_WINDOWS)

#elif defined(ANVIL_PLATFORM_OSX)
#include <OpenGL/gl3.h>
#endif

#include "third_party/stb/stb_image.h"

template<typename factory_t> 
class GlObject : public Noncopyable
{
	mutable GLuint handle = 0;
	std::string name;
public:
	GlObject() {}
	GlObject(GLuint h) : handle(g) {}
	~GlObject() { if (handle) factory_t::destroy(handle); }
	GlObject & operator = (GlObject && r) { std::swap(handle, r.handle); return *this; }
	operator GLuint () { if (!handle) factory_t::create(handle); return handle; }
	GlObject & operator = (GLuint & other) { handle = other; return *this; }
	void set_name(const std::string & n) { name = n; }
	std::string name() const { return name; }
};

struct GlBufferFactory { static void create(GLuint & x) { glGenBuffers(1, &x); }; static void destroy(GLuint x) { glDeleteBuffers(1, &x); }; };
struct GlTextureFactory { static void create(GLuint & x) { glGenTextures(1, &x); }; static void destroy(GLuint x) { glDeleteTextures(1, &x); }; };
struct GlVertexArrayFactory { static void create(GLuint & x) { glGenVertexArrays(1, &x); }; static void destroy(GLuint x) { glDeleteVertexArrays(1, &x); }; };
struct GlRenderbufferFactory { static void create(GLuint & x) { glGenRenderbuffers(1, &x); }; static void destroy(GLuint x) { glDeleteRenderbuffers(1, &x); }; };
struct GlFramebufferFactory { static void create(GLuint & x) { glGenFramebuffers(1, &x); }; static void destroy(GLuint x) { glDeleteFramebuffers(1, &x); }; };
struct GlQueryFactory { static void create(GLuint & x) { glGenQueries(1, &x); }; static void destroy(GLuint x) { glDeleteQueries(1, &x); }; };
struct GlSamplerFactory { static void create(GLuint & x) { glGenSamplers(1, &x); }; static void destroy(GLuint x) { glDeleteSamplers(1, &x); }; };
struct GlTransformFeedbacksFactory { static void create(GLuint & x) { glGenTransformFeedbacks(1, &x); }; static void destroy(GLuint x) { glDeleteTransformFeedbacks(1, &x); }; };

typedef GlObject<GlBufferFactory> GlBufferObject;
typedef GlObject<GlTextureFactory> GlTextureObject;
typedef GlObject<GlVertexArrayFactory> GlVertexArrayObject;
typedef GlObject<GlRenderbufferFactory> GlRenderbufferObject;
typedef GlObject<GlFramebufferFactory> GlFramebufferObject;
typedef GlObject<GlQueryFactory> GlQueryObject;
typedef GlObject<GlSamplerFactory> GlSamplerObject;
typedef GlObject<GlTransformFeedbacksFactory> GlTransformFeedbacksObject;

namespace
{
    inline void compile_shader(GLuint program, GLenum type, const char * source)
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
}

namespace avl
{
    ///////////////
    //   Utils   //
    ///////////////
    
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
    

	//////////////////
	//   GlBuffer   //
	//////////////////

	struct GlBuffer : public GlBufferObject, public Noncopyable
	{
		GLsizeiptr size;
		GlBuffer() {}
		GlBuffer(GlBuffer && r) : GlBuffer() { *this = std::move(r); }
		GlBuffer & operator = (GlBuffer && r) { std::swap(size, r.size); return *this; }
		void set_buffer_data(const GLenum target, const GLsizeiptr size, const GLvoid * data, const GLenum usage) { glNamedBufferDataEXT(target, size, data, usage); this->size = size; }
		void set_buffer_data(const GLenum target, const std::vector<GLubyte> & bytes, const GLenum usage) { set_buffer_data(target, bytes.size(), bytes.data(), usage); }
	};

	////////////////////////
	//   GlRenderbuffer   //
	////////////////////////

	struct GlRenderbuffer : public GlRenderbufferObject, public Noncopyable
	{
		int2 size;
		GlRenderbuffer() {};
		GlRenderbuffer(int2 size) : size(size) {}
		GlRenderbuffer(GlRenderbuffer && r) : GlRenderbuffer() { *this = std::move(r); }
		GlRenderbuffer & operator = (GlRenderbuffer && r) { std::swap(size, r.size); return *this; }
	};


	///////////////////////
	//   GlFramebuffer   //
	///////////////////////

	struct GlFramebuffer : public GlFramebufferObject, public Noncopyable
	{
		float3 size;
		GlFramebuffer() {}
		GlFramebuffer(float2 s) : size(s.x, s.y, 0) {}
		GlFramebuffer(float3 s) : size(s) {}
		GlFramebuffer(GlFramebuffer && r) : GlFramebuffer() { *this = std::move(r); }
		GlFramebuffer & operator = (GlFramebuffer && r) { std::swap(size, r.size); return *this; }
		void check_complete() { if (glCheckNamedFramebufferStatusEXT(*this, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) throw std::runtime_error("fbo incomplete"); }
	};

    ///////////////////
    //   GlTexture   //
    ///////////////////
    
    struct GlTexture2D : public GlTextureObject, public Noncopyable
    {
        int2 size;

		GlTexture2D() {}
		GlTexture2D(int2 sz) : size(sz) {}
		GlTexture2D(GlTexture2D && r) : GlTexture2D() { *this = std::move(r); }
		GlTexture2D & operator = (GlTexture2D && r) { std::swap(size, r.size); return *this; }

		void setup(GLsizei width, GLsizei height, GLenum internal_fmt, GLenum format, GLenum type, const GLvoid * pixels, bool createMipmap = false)
		{
			glTextureImage2DEXT(*this, GL_TEXTURE_2D, 0, internal_fmt, width, height, 0, format, type, pixels);
			if (createMipmap) glGenerateTextureMipmapEXT(*this, GL_TEXTURE_2D);
			glTextureParameteriEXT(*this, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTextureParameteriEXT(*this, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, createMipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
			glTextureParameteriEXT(*this, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTextureParameteriEXT(*this, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTextureParameteriEXT(*this, GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		}
    };
    
    inline GlTexture2D load_image(const std::string & path)
    {
        auto binaryFile = read_file_binary(path);
        
        int width, height, nBytes;
        auto data = stbi_load_from_memory(binaryFile.data(), (int) binaryFile.size(), &width, &height, &nBytes, 0);
        
		GlTexture2D tex;
        switch(nBytes)
        {
            case 3: tex.setup(width, height, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE, data, true); break;
            case 4: tex.setup(width, height, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, data, true); break;
        }
        
        stbi_image_free(data);
        return tex;
    }

    inline std::vector<uint8_t> load_image_data(const std::string & path)
    {
        auto binaryFile = read_file_binary(path);
        
        int width, height, nBytes;
        auto data = stbi_load_from_memory(binaryFile.data(), (int) binaryFile.size(), &width, &height, &nBytes, 0);
        std::vector<uint8_t> d(width * height * nBytes);
        memcpy(d.data(), data, nBytes * width * height);
        stbi_image_free(data);
        return d;
    }
    
    /////////////////////
    //   GlTexture3D   //
    /////////////////////
    
	// As either a 3D texture or 2D array
	struct GlTexture3D : public GlTextureObject, public Noncopyable
	{
		int3 size;

		GlTexture3D() {}
		GlTexture3D(int3 sz) : size(sz) {}
		GlTexture3D(GlTexture2D && r) : GlTexture3D() { *this = std::move(r); }
		GlTexture3D & operator = (GlTexture3D && r) { std::swap(size, r.size); return *this; }

		void setup(GLenum target, GLsizei width, GLsizei height, GLsizei depth, GLenum internal_fmt, GLenum format, GLenum type, const GLvoid * pixels)
		{
			glTextureImage3DEXT(*this, target, 0, internal_fmt, width, height, depth, 0, format, type, pixels);
			glTextureParameteriEXT(*this, target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTextureParameteriEXT(*this, target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTextureParameteriEXT(*this, target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTextureParameteriEXT(*this, target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTextureParameteriEXT(*this, target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		}
	};
    
    //////////////////
    //   GlShader   //
    //////////////////
    
    class GlShader : public Noncopyable
    {
        GLuint program;
        bool enabled = false;
        void check() const { if (!enabled) throw std::runtime_error("shader not enabled"); };
        
    public:
        
        GlShader() : program() {}
        GlShader(const std::string & vertexShader, const std::string & fragmentShader, const std::string & geometryShader = "")
        {
            program = glCreateProgram();

            compile_shader(program, GL_VERTEX_SHADER, vertexShader.c_str());
            compile_shader(program, GL_FRAGMENT_SHADER, fragmentShader.c_str());
            if (geometryShader.length() != 0) ::compile_shader(program, GL_GEOMETRY_SHADER, geometryShader.c_str());
            
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
        void uniform(const std::string & name, const float3x3 & mat) const { check(); glUniformMatrix3fv(get_uniform_location(name), 1, GL_FALSE, &mat.x.x); }
        void uniform(const std::string & name, const float4x4 & mat) const { check(); glUniformMatrix4fv(get_uniform_location(name), 1, GL_FALSE, &mat.x.x); }
        
        void uniform(const std::string & name, const int elements, const std::vector<int> & scalar) const { check(); glUniform1iv(get_uniform_location(name), elements, scalar.data()); }
        void uniform(const std::string & name, const int elements, const std::vector<float> & scalar) const { check(); glUniform1fv(get_uniform_location(name), elements, scalar.data()); }
        void uniform(const std::string & name, const int elements, const std::vector<float2> & vec) const { check(); glUniform2fv(get_uniform_location(name), elements, &vec[0].x); }
        void uniform(const std::string & name, const int elements, const std::vector<float3> & vec) const { check(); glUniform3fv(get_uniform_location(name), elements, &vec[0].x); }
        void uniform(const std::string & name, const int elements, const std::vector<float3x3> & mat) const { check(); glUniformMatrix3fv(get_uniform_location(name), elements, GL_FALSE, &mat[0].x.x); }
        void uniform(const std::string & name, const int elements, const std::vector<float4x4> & mat) const { check(); glUniformMatrix4fv(get_uniform_location(name), elements, GL_FALSE, &mat[0].x.x); }
        
        void texture(const std::string & name, int unit, GLuint texId, GLenum textureTarget) const
        {
            check();
            glUniform1i(get_uniform_location(name), unit);
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(textureTarget, texId);
        }
        
        void texture(const char * name, int unit, GlTexture2D tex) const { texture(name, unit, tex, GL_TEXTURE_2D); }
        void texture(const char * name, int unit, GLenum target, GlTexture3D tex) const { texture(name, unit, tex, target); }
        
        void bind() { if (program > 0) enabled = true; glUseProgram(program); }
        void unbind() { enabled = false; glUseProgram(0); }
    };

    ////////////////
    //   GlMesh   //
    ////////////////
    
    class GlMesh : public Noncopyable
    {
        enum { MAX_ATTRIBUTES = 8 };
        struct Attribute { bool is_instance; GLint size; GLenum type; GLboolean normalized; GLsizei stride; const GLvoid * pointer; } attributes[MAX_ATTRIBUTES];
        
        GlBuffer vertexBuffer, instanceBuffer, indexBuffer;
        
        GLuint vertexArrayHandle;
        GLenum mode = GL_TRIANGLES;
        GLenum indexType = 0;
        GLsizei vertexStride = 0, instanceStride = 0;
        
    public:
        
        GlMesh() { memset(attributes,0 ,sizeof(attributes)); glGenVertexArrays(1, &vertexArrayHandle); }

        GlMesh(GlMesh && r) : GlMesh() { *this = std::move(r); }
        ~GlMesh() {};
        
        GlMesh & operator = (GlMesh && r)
        {
            char buffer[sizeof(GlMesh)];
            memcpy(buffer, this, sizeof(buffer));
            memcpy(this, &r, sizeof(buffer));
            memcpy(&r, buffer, sizeof(buffer));
            return *this;
        }
        
        void set_non_indexed(GLenum newMode)
        {
            mode = newMode;
            indexBuffer = {};
            indexType = 0;
        }
        
        void draw_elements(int instances = 0) const
        {
            GLsizei vertexCount = vertexStride ? ((int) vertexBuffer.size() / vertexStride) : 0;
            
            GLsizei indexCount = [&]() -> GLsizei
            {
                if (indexType == GL_UNSIGNED_BYTE) return ((int)indexBuffer.size() / sizeof(GLubyte));
                if (indexType == GL_UNSIGNED_SHORT) return ((int)indexBuffer.size() / sizeof(GLushort));
                if (indexType == GL_UNSIGNED_INT) return ((int)indexBuffer.size() / sizeof(GLuint));
                else return 0;
            }();
            
            if (vertexCount)
            {
                // BEGIN stuff that only needs to be done once
                glBindVertexArray(vertexArrayHandle);
                for (GLuint index = 0; index < MAX_ATTRIBUTES; ++index)
                {
                    if (attributes[index].size)
                    {
                        (attributes[index].is_instance ? instanceBuffer : vertexBuffer).bind(GL_ARRAY_BUFFER);
                        glVertexAttribPointer(index, attributes[index].size, attributes[index].type, attributes[index].normalized, attributes[index].stride, attributes[index].pointer); // AttribPointer is relative to currently point ARRAY_BUFFER
                        glVertexAttribDivisor(index, attributes[index].is_instance ? 1 : 0);
                        glEnableVertexAttribArray(index);
                        
                    }
                }
                
                if (indexCount)
                {
                    indexBuffer.bind(GL_ELEMENT_ARRAY_BUFFER);
                    if (instances) glDrawElementsInstanced(mode, indexCount, indexType, 0, instances);
                    else glDrawElements(mode, indexCount, indexType, nullptr);
                    indexBuffer.unbind(GL_ELEMENT_ARRAY_BUFFER);
                }
                else
                {
                    if (instances) glDrawArraysInstanced(mode, 0, vertexCount, instances);
                    else glDrawArrays(mode, 0, vertexCount);
                }
                
                for (GLuint index = 0; index < MAX_ATTRIBUTES; ++index)
                {
                    glDisableVertexAttribArray(index);
                }
                
                glBindVertexArray(0);
            }
        }
        
        void set_vertex_data(GLsizeiptr size, const GLvoid * data, GLenum usage)
        {
            vertexBuffer.set_buffer_data(GL_ARRAY_BUFFER, size, data, usage);
        }
        
        void set_instance_data(GLsizeiptr size, const GLvoid * data, GLenum usage)
        {
            instanceBuffer.set_buffer_data(GL_ARRAY_BUFFER, size, data, usage);
        }
        
        void set_index_data(GLenum mode, GLenum type, GLsizei count, const GLvoid * data, GLenum usage)
        {
            size_t elementSize;
            switch(type)
            {
                case GL_UNSIGNED_BYTE: elementSize = sizeof(uint8_t); break;
                case GL_UNSIGNED_SHORT: elementSize = sizeof(uint16_t); break;
                case GL_UNSIGNED_INT: elementSize = sizeof(uint32_t); break;
                default: throw std::logic_error("unknown element type"); break;
            }
            indexBuffer.set_buffer_data(GL_ELEMENT_ARRAY_BUFFER, elementSize * count, data, usage);
            this->mode = mode;
            indexType = type;
        }
        
        void set_attribute(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid * pointer)
        {
            attributes[index] = {false, size, type, normalized, stride, pointer};
            vertexStride = stride;
        }
        
        void set_instance_attribute(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid * pointer)
        {
            attributes[index] = {true, size, type, normalized, stride, pointer };
            instanceStride = stride;
        }
        
        void set_indices(GLenum mode, GLsizei count, const uint8_t * indices, GLenum usage) { set_index_data(mode, GL_UNSIGNED_BYTE, count, indices, usage); }
        void set_indices(GLenum mode, GLsizei count, const uint16_t * indices, GLenum usage) { set_index_data(mode, GL_UNSIGNED_SHORT, count, indices, usage); }
        void set_indices(GLenum mode, GLsizei count, const uint32_t * indices, GLenum usage) { set_index_data(mode, GL_UNSIGNED_INT, count, indices, usage); }
        
        template<class T> void set_vertices(size_t count, const T * vertices, GLenum usage) { set_vertex_data(count * sizeof(T), vertices, usage); }
        template<class T> void set_vertices(const std::vector<T> & vertices, GLenum usage) { set_vertices(vertices.size(), vertices.data(), usage); }
        template<class T, int N> void set_vertices(const T (&vertices)[N], GLenum usage) { set_vertices(N, vertices, usage); }
        
        template<class V>void set_attribute(GLuint index, float V::*field) { set_attribute(index, 1, GL_FLOAT, GL_FALSE, sizeof(V), &(((V*)0)->*field)); }
        template<class V, int N> void set_attribute(GLuint index, linalg::vec<float,N> V::*field) { set_attribute(index, N, GL_FLOAT, GL_FALSE, sizeof(V), &(((V*)0)->*field)); }
        
        template<class T> void set_elements(GLsizei count, const linalg::vec<T,2> * elements, GLenum usage) { set_indices(GL_LINES, count * 2, &elements->x, GL_STATIC_DRAW); }
        template<class T> void set_elements(GLsizei count, const linalg::vec<T,3> * elements, GLenum usage) { set_indices(GL_TRIANGLES, count * 3, &elements->x, GL_STATIC_DRAW); }
        template<class T> void set_elements(GLsizei count, const linalg::vec<T,4> * elements, GLenum usage) { set_indices(GL_QUADS, count * 4, &elements->x, GL_STATIC_DRAW); }
        
        template<class T> void set_elements(const std::vector<T> & elements, GLenum usage) { set_elements((GLsizei)elements.size(), elements.data(), usage); }
        
        template<class T, int N> void set_elements(const T (&elements)[N], GLenum usage) { set_elements(N, elements, usage); }
    };
    
}

#endif // end AVL_GL_API_H
