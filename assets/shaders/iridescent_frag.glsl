// https://vimeo.com/83798053

#version 330

in vec3 v_position, v_normal;
in vec2 v_texcoord;
out vec4 f_color;

uniform float u_iridescentRatio = 1.0;

float map(float value, float oMin, float oMax, float iMin, float iMax)
{
    return iMin + ((value - oMin) / (oMax - oMin)) * (iMax - iMin);
}

float diNoise(vec3 pos)
{
    float muliplier = 0.05;
    float offset = 25; 
    return(
        sin(pos.x * muliplier * 2 + 12 + offset) + cos(pos.z * muliplier + 21 + offset) *
        sin(pos.y * muliplier * 2 + 23 + offset) + cos(pos.y * muliplier + 32 + offset) *
        sin(pos.z * muliplier * 2 + 34 + offset) + cos(pos.x * muliplier + 43 + offset));
}

vec3 compute_iridescence(float orientation, vec3 position)
{
    vec3 color;
    float frequency = 8.0;
    float offset = 4.0;
    float noiseInc = 1.0;

    color.x = abs(cos(orientation * frequency + diNoise(position) * noiseInc + 1 + offset));
    color.y = abs(cos(orientation * frequency + diNoise(position) * noiseInc + 2 + offset));
    color.z = abs(cos(orientation * frequency + diNoise(position) * noiseInc + 3 + offset));

    return color;
}

void main()
{   
    vec3 vert = gl_FrontFacing ? normalize(-v_position) : normalize(v_position);

    float facingRatio = dot(v_normal, vert);

    vec4 iridescentColor = vec4(compute_iridescence(facingRatio, v_position), 1.0) * 
                            map(pow(1 - facingRatio, 1.0/0.75), 0.0, 1.0, 0.1, 1);

    vec4 transparentIridescence = iridescentColor * u_iridescentRatio;
    vec4 nonTransprentIridescence = vec4(mix(vec3(1, 0, 1), iridescentColor.rgb, u_iridescentRatio), 1.0);

    f_color = gl_FrontFacing ? transparentIridescence : transparentIridescence;
}
