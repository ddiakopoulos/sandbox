#version 330
uniform sampler2D u_texture;
in vec2 texCoord;
out vec4 f_color;
void main()
{
    f_color = texture(u_texture, texCoord);
}