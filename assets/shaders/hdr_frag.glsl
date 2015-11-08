#version 330

uniform sampler2D u_Texture;
uniform float u_Exposure;

in vec3 position;
in vec2 texCoord;

out vec4 f_color;

vec3 gamma(vec3 v)
{
    return pow(v, vec3(1 / 2.2));
}

void main()
{
    vec3 hdrColor = texture(u_Texture, texCoord).rgb;
    
    // Exposure tone mapping
    vec3 mapped = vec3(1.0) - exp(-hdrColor * u_Exposure);
    
    // Gamma corrected
    f_color = vec4(gamma(clamp(mapped, 0, 1)), 1.0);
}