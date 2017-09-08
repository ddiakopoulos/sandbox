#ifndef texture_view_h
#define texture_view_h

#include <vector>
#include "gl-api.hpp"
#include "util.hpp"

static const char s_textureVert[] = R"(#version 330
    layout(location = 0) in vec3 position;
    layout(location = 3) in vec2 uvs;
    uniform mat4 u_mvp;
    out vec2 texCoord;
    void main()
    {
        texCoord = uvs;
        gl_Position = u_mvp * vec4(position.xy, 0.0, 1.0);
    
    }
)";

static const char s_textureVertFlip[] = R"(#version 330
    layout(location = 0) in vec3 position;
    layout(location = 3) in vec2 uvs;
    uniform mat4 u_mvp;
    out vec2 texCoord;
    void main()
    {
        texCoord = vec2(uvs.x, 1 - uvs.y);
        gl_Position = u_mvp * vec4(position.xy, 0.0, 1.0);
    
    }
)";

static const char s_textureFrag[] = R"(#version 330
    uniform sampler2D u_texture;
    in vec2 texCoord;
    out vec4 f_color;
    void main()
    {
        vec4 sample = texture(u_texture, texCoord);
        f_color = vec4(sample.r, sample.r, sample.r, 1.0); 
    }
)";

///////////////////////////////////////////////////////////////////////////////////////

static const char s_textureVert3D[] = R"(#version 330
    layout(location = 0) in vec3 position;
    layout(location = 3) in vec2 uvs;
    uniform mat4 u_mvp = mat4(1.0);
    out vec2 v_texcoord;
    void main()
    {
        v_texcoord = uvs;
        gl_Position = u_mvp * vec4(position.xy, 0.0, 1.0);
    }
)";

static const char s_textureFrag3D[] = R"(#version 330
    uniform sampler2DArray u_texture;
    uniform int u_slice;
    in vec2 v_texcoord;
    out vec4 f_color;
    void main()
    {
        vec4 sample = texture(u_texture, vec3(v_texcoord, float(u_slice)));
        f_color = sample; //vec4(sample.r, sample.r, sample.r, 1.0); // temp hack for debugging
    }
)";

namespace avl
{

    struct GLTextureView : public Noncopyable
    {
        GlShader program;
        GlMesh mesh = make_fullscreen_quad_screenspace();
        
        GLTextureView(bool flip = false)
        {
            program = flip ? GlShader(s_textureVertFlip, s_textureFrag) : GlShader(s_textureVert, s_textureFrag);
        }
        
        void draw(const Bounds2D & rect, const float2 windowSize, const GLuint tex)
        {
            const float4x4 projection = make_orthographic_matrix(0.0f, windowSize.x, windowSize.y, 0.0f, -1.0f, 1.0f);
            float4x4 model = make_scaling_matrix({ rect.width(), rect.height(), 0.f });
            model = mul(make_translation_matrix({ rect.min().x, rect.min().y, 0.f }), model);
            program.bind();
            program.uniform("u_mvp", mul(projection, model));
            program.texture("u_texture", 0, tex, GL_TEXTURE_2D);
            mesh.draw_elements();
            program.unbind();
        }
        
    };
    
    class GLTextureView3D : public Noncopyable
    {
        GlShader program;
        GlMesh mesh = make_fullscreen_quad_screenspace(); 
    public:
        GLTextureView3D() { program = GlShader(s_textureVert3D, s_textureFrag3D); }
        void draw(const Bounds2D & rect, const float2 windowSize, const GLuint tex, const GLenum target, const int slice)
        {
            const float4x4 projection = make_orthographic_matrix(0.0f, windowSize.x, windowSize.y, 0.0f, -1.0f, 1.0f);
            float4x4 model = make_scaling_matrix({ rect.width(), rect.height(), 0.f });
            model = mul(make_translation_matrix({rect.min().x, rect.min().y, 0.f}), model);
            program.bind();
            program.uniform("u_mvp", mul(projection, model));
            program.uniform("u_slice", slice);
            program.texture("u_texture", 0, tex, target);
            mesh.draw_elements();
            program.unbind();
        }
    };
 
}

#endif // texture_view_h
