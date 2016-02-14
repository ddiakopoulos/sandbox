#ifndef texture_view_h
#define texture_view_h

#include <vector>
#include "GL_API.hpp"
#include "util.hpp"

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

namespace avl
{
    
    struct GLTextureView : public Noncopyable
    {
        GlShader program;
        GlMesh mesh;
        GLuint texture;
        
        GLTextureView(GLuint tex) : texture(tex)
        {
            Geometry g;
            g.vertices = { {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f} };
            g.texCoords = { {0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f} };
            g.faces = {{0, 1, 2}, {3, 4, 5}};
            mesh = make_mesh_from_geometry(g);
            program = GlShader(s_textureVert, s_textureFrag);
        }
        
        void draw(Bounds rect, int2 windowSize)
        {
            program.bind();
            
            const float4x4 projection = make_orthographic_perspective_matrix(0.0f, windowSize.x, windowSize.y, 0.0f, -1.0f, 1.0f);
            
            float4x4 model = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
            model = mul(make_scaling_matrix({ (float) rect.width(), (float) rect.height(), 0.f}), model);
            model = mul(make_translation_matrix({(float) rect.x0, (float) rect.y0, 0.f}), model);
            program.uniform("u_model", model);
            program.uniform("u_projection", projection);
            program.texture("u_texture", 0, texture, GL_TEXTURE_2D);
            
            mesh.draw_elements();
            
            program.unbind();
        }
        
    };
        
}

#endif // texture_view_h
