#version 330

struct DirectionalLight
{
    vec3 color;
    vec3 direction;
};

in vec3 v_world_position;
in vec2 v_texcoord;
in vec3 v_normal;
in vec4 v_camera_directional_light;

out vec4 f_color;

uniform DirectionalLight u_directionalLight;

uniform sampler2D s_directionalShadowMap;
uniform float u_shadowMapBias;
uniform vec2 u_shadowMapTexelSize;

float sample_shadow_map(sampler2D shadowMap, vec2 uv, float compare)
{
    float depth = texture(shadowMap, uv).x;
    if (depth < compare) return 0.5;
    else return 1.0;
}

float shadow_map_linear(sampler2D shadowMap, vec2 coords, float compare, vec2 texelSize)
{
    vec2 pixelPos = coords/texelSize + vec2(0.5);
    vec2 fracPart = fract(pixelPos);
    vec2 startTexel = (pixelPos - fracPart) * texelSize;
    
    float blTexel = sample_shadow_map(shadowMap, startTexel, compare);
    float brTexel = sample_shadow_map(shadowMap, startTexel + vec2(texelSize.x, 0.0), compare);
    float tlTexel = sample_shadow_map(shadowMap, startTexel + vec2(0.0, texelSize.y), compare);
    float trTexel = sample_shadow_map(shadowMap, startTexel + texelSize, compare);
    
    float mixA = mix(blTexel, tlTexel, fracPart.y);
    float mixB = mix(brTexel, trTexel, fracPart.y);
    
    return mix(mixA, mixB, fracPart.x);
}

float shadow_map_pcf(sampler2D shadowMap, vec2 coords, float compare, vec2 texelSize)
{
    const float NUM_SAMPLES = 3.0f;
    const float SAMPLES_START = (NUM_SAMPLES-1.0f)/2.0f;
    const float NUM_SAMPLES_SQUARED = NUM_SAMPLES*NUM_SAMPLES;

    float result = 0.0f;
    for(float y = -SAMPLES_START; y <= SAMPLES_START; y += 1.0f)
    {
        for(float x = -SAMPLES_START; x <= SAMPLES_START; x += 1.0f)
        {
            vec2 coordsOffset = vec2(x,y)*texelSize;
            result += shadow_map_linear(shadowMap, coords + coordsOffset, compare, texelSize);
        }
    }
    return result/NUM_SAMPLES_SQUARED;
}

bool in_range(float val)
{
    return val >= 0.0 && val <= 1.0;
}

vec4 calculate_directional_light(DirectionalLight dl, vec4 cameraSpacePosition)
{
    float light = max(dot(v_normal, dl.direction), 0);
    return vec4(dl.color * light, 1.0);
}

float calculate_directional_light_shadow(vec4 cameraSpacePosition)
{
    vec3 projectedCoords = cameraSpacePosition.xyz / cameraSpacePosition.w;

    vec2 uvCoords;
    uvCoords.x = 0.5 * projectedCoords.x + 0.5;
    uvCoords.y = 0.5 * projectedCoords.y + 0.5;
    float z = 0.5 * projectedCoords.z + 0.5;
    
    if (!in_range(uvCoords.x) || !in_range(uvCoords.y) || !in_range(z))
    {
        return 1.0;
    }
    
    return shadow_map_pcf(s_directionalShadowMap, uvCoords, z - u_shadowMapBias, u_shadowMapTexelSize);
}

void main()
{
    vec4 ambient = vec4(0.0, 0.0, 0.0, 1.0);
    vec4 totalLighting = vec4(0.0, 0.0, 0.0, 0.0);
    totalLighting += calculate_directional_light(u_directionalLight, v_camera_directional_light);
    f_color = clamp(totalLighting + ambient, 0.0, 1.0);
}