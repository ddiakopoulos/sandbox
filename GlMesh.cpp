#include "GlMesh.hpp"

using namespace avl;

void GlMesh::draw_elements() const
{
    GLsizei vertexCount = vstride ? ((int) vbo.size() / vstride) : 0;
    
    GLsizei indexCount = [&]()
    {
        if (indexType == GL_UNSIGNED_BYTE) return ((int)ibo.size() / sizeof(GLubyte));
        if (indexType == GL_UNSIGNED_SHORT) return ((int)ibo.size() / sizeof(GLushort));
        if (indexType == GL_UNSIGNED_INT) return ((int)ibo.size() / sizeof(GLuint));
        else return (size_t) 0;
    }();

    if (vertexCount)
    {
        vbo.bind(GL_ARRAY_BUFFER);
        for(GLuint index = 0; index < MAX_ATTRIBUTES; ++index)
        {
            if(attributes[index].size)
            {
                glBindVertexArray(vao);
                glEnableVertexAttribArray(index);
                glVertexAttribPointer(index, attributes[index].size, attributes[index].type, attributes[index].normalized, attributes[index].stride, attributes[index].pointer);
            }
        }
        if (indexCount)
        {
            ibo.bind(GL_ELEMENT_ARRAY_BUFFER);
            glDrawElements(mode, indexCount, indexType, nullptr);
            ibo.unbind(GL_ELEMENT_ARRAY_BUFFER);
        }
        else
        {
            glDrawArrays(mode, 0, vertexCount);
        }
        for (GLuint index = 0; index < MAX_ATTRIBUTES; ++index)
        {
            glDisableVertexAttribArray(index);
        }
        vbo.unbind(GL_ARRAY_BUFFER);
    }
}

void GlMesh::set_vertex_data(GLsizeiptr size, const GLvoid * data, GLenum usage)
{
    vbo.set_buffer_data(GL_ARRAY_BUFFER, size, data, usage);
}

void GlMesh::set_index_data(GLenum mode, GLenum type, GLsizei count, const GLvoid * data, GLenum usage)
{
    size_t elementSize;
    switch(type)
    {
        case GL_UNSIGNED_BYTE: elementSize = sizeof(uint8_t); break;
        case GL_UNSIGNED_SHORT: elementSize = sizeof(uint16_t); break;
        case GL_UNSIGNED_INT: elementSize = sizeof(uint32_t); break;
        default: throw std::logic_error("unknown element type"); break;
    }
    ibo.set_buffer_data(GL_ELEMENT_ARRAY_BUFFER, elementSize * count, data, usage);
    this->mode = mode;
    indexType = type;
}

void GlMesh::set_attribute(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid * pointer)
{
    attributes[index] = {size, type, normalized, stride, pointer};
    vstride = stride;
}
