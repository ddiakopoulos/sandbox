#version 330

uniform float u_time;
uniform vec3 u_eye;
uniform vec3 u_scale = vec3(0.25, 0.25, 0.25);

uniform sampler2D s_diffuseTextureA, s_diffuseTextureB;

in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_texcoord;
in vec3 v_eyeDir;

out vec4 f_color;

const float yCapHeight = 1.0;
const float yCapTransition = 0.3;
const float blendSharpness = 2.0;

void main()
{
    // calculate a blend factor for triplanar mapping. Sharpness should be obvious.
    vec3 bf = pow(abs(v_normal), vec3(blendSharpness));
    bf /= dot(bf, vec3(1));

    // compute texcoords for each axis based on world position of the fragment
    vec2 tx = v_world_position.yz * u_scale.x;
    vec2 ty = v_world_position.zx * u_scale.y;
    vec2 tz = v_world_position.xy * u_scale.z;

    vec4 cx = texture(s_diffuseTextureA, tx);
    vec4 cy = texture(s_diffuseTextureB, ty);
    vec4 cz = texture(s_diffuseTextureA, tz); 

    float h = clamp(v_world_position.y - yCapHeight, 0, yCapTransition) / yCapTransition;
    vec3 yDiff = cy.rgb * h;

    f_color = vec4((cx.rgb * bf.x) + (yDiff * bf.y) + (cz.rgb * bf.z), 1);
}