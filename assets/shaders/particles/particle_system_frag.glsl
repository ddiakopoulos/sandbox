#version 330

uniform sampler2D u_outerTex;
uniform sampler2D u_innerTex;

in vec3 v_position;
in vec2 v_texcoord;

out vec4 f_color;

void main() 
{
    vec3 norm = vec3(0,0,1);
    vec3 vU = normalize(v_position);
    vec3 r = reflect(vU, norm);
    float m = 2.0 * sqrt(r.x * r.x + r.y * r.y + ( r.z + 1.0 ) * ( r.z+1.0 ));
    vec2 vN = vec2( r.x / m + 0.5,  r.y / m + 0.5 );
    f_color = (texture2D(u_outerTex, vN) * texture2D(u_innerTex, v_texcoord) * 1.5);
}