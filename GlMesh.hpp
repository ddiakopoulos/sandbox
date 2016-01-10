// See COPYING file for attribution information

#ifndef glmesh_h
#define glmesh_h

#include "linear_algebra.hpp"
#include "math_util.hpp"
#include "geometric.hpp"
#include "util.hpp"
#include "GlShared.hpp"

#include <vector>

namespace avl
{
    
    class GlMesh : public Noncopyable
    {
        enum { MAX_ATTRIBUTES = 8 };
        struct Attribute { GLint size; GLenum type; GLboolean normalized; GLsizei stride; const GLvoid * pointer; } attributes[MAX_ATTRIBUTES];
        
        GlBuffer vbo, ibo;
        GLenum mode = GL_TRIANGLES;
        GLenum indexType = 0;
        GLsizei vstride = 0;
        
        GLuint vao;
        
    public:
        
        GlMesh() { memset(attributes,0,sizeof(attributes)); glGenVertexArrays(1, &vao); }
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
        
        void set_non_indexed(GLenum mode)
        {
            this->mode = mode;
            ibo = {};
            indexType = 0;
        }

        void draw_elements() const;
        
        void set_vertex_data(GLsizeiptr size, const GLvoid * data, GLenum usage);
        void set_index_data(GLenum mode, GLenum type, GLsizei count, const GLvoid * data, GLenum usage);
        void set_attribute(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid * pointer);
        
        void set_indices(GLenum mode, GLsizei count, const uint8_t * indices, GLenum usage) { set_index_data(mode, GL_UNSIGNED_BYTE, count, indices, usage); }
        void set_indices(GLenum mode, GLsizei count, const uint16_t * indices, GLenum usage) { set_index_data(mode, GL_UNSIGNED_SHORT, count, indices, usage); }
        void set_indices(GLenum mode, GLsizei count, const uint32_t * indices, GLenum usage) { set_index_data(mode, GL_UNSIGNED_INT, count, indices, usage); }
        
        template<class T> void set_vertices(size_t count, const T * vertices, GLenum usage) { set_vertex_data(count * sizeof(T), vertices, usage); }
        template<class T> void set_vertices(const std::vector<T> & vertices, GLenum usage) { set_vertices(vertices.size(), vertices.data(), usage); }
        template<class T, int N> void set_vertices(const T (&vertices)[N], GLenum usage) { set_vertices(N, vertices, usage); }
        
        template<class V>void set_attribute(GLuint index, float V::*field) { set_attribute(index, 1, GL_FLOAT, GL_FALSE, sizeof(V), &(((V*)0)->*field)); }
        template<class V, int N> void set_attribute(GLuint index, vec<float,N> V::*field) { set_attribute(index, N, GL_FLOAT, GL_FALSE, sizeof(V), &(((V*)0)->*field)); }
        
        template<class T> void set_elements(GLsizei count, const vec<T,2> * elements, GLenum usage) { set_indices(GL_LINES, count * 2, &elements->x, GL_STATIC_DRAW); }
        template<class T> void set_elements(GLsizei count, const vec<T,3> * elements, GLenum usage) { set_indices(GL_TRIANGLES, count * 3, &elements->x, GL_STATIC_DRAW); }
        template<class T> void set_elements(GLsizei count, const vec<T,4> * elements, GLenum usage) { set_indices(GL_QUADS, count * 4, &elements->x, GL_STATIC_DRAW); }
        
        template<class T> void set_elements(const std::vector<T> & elements, GLenum usage) { set_elements((GLsizei)elements.size(), elements.data(), usage); }
        
        template<class T, int N> void set_elements(const T (&elements)[N], GLenum usage) { set_elements(N, elements, usage); }
    };
    
}

#endif // glmesh_h
