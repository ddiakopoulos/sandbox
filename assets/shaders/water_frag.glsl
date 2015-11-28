#version 330

#define FOG_DENSITY 0.025
#define FOG_COLOR vec3(1.0)

in vec2 vTexCoord;
in vec3 vPosition;

out vec4 f_color;

uniform sampler2D u_reflectionTexture;
uniform sampler2D u_depthTexture;

uniform mat3 u_modelMatrixIT;
uniform mat4 u_modelMatrix;

uniform vec2 u_resolution;
uniform vec3 u_lightPosition;
uniform float u_near;
uniform float u_far;

float linearize_depth(float depth) 
{
    return 2.0 * u_near * u_far / (u_far + u_near - (2.0 * depth - 1.0) * (u_far - u_near));
}

float exp_fog(const float dist, const float density) 
{
    float d = density * dist;
    return exp2(d * d * -1.44);
}

void main(void) 
{
    vec3 p0 = dFdx(vPosition);
    vec3 p1 = dFdy(vPosition);
    vec3 n = u_modelMatrixIT * normalize(cross(p0, p1));

    vec3 surfaceColor = vec3(0.88, 0.88, 0.88);
    float ambientIntensity = 0.75;

    vec3 surfaceToLight = normalize(u_modelMatrix * vec4(u_lightPosition, 0.0)).xyz;
    vec3 surfaceToCamera = normalize(-u_modelMatrix * vec4(vPosition, 0.0)).xyz;

    vec3 ambient = ambientIntensity * surfaceColor;
    float diffuseCoefficient = max(0.0, dot(n, surfaceToLight));
    vec3 diffuse = diffuseCoefficient * surfaceColor;

    vec3 lightFactor = ambient + diffuse;

    vec2 uv = (gl_FragCoord.xy) / u_resolution;
    vec2 reflectionUV = vec2(uv.x, 1.0 - uv.y);

    float depth = texture(u_depthTexture, uv).x;
    float waterDepth = linearize_depth(depth) - linearize_depth(gl_FragCoord.z);

    f_color = vec4(lightFactor, 0.4 + 0.2 * diffuseCoefficient);

    // mix with reflection texture
    f_color.rgb = mix(texture(u_reflectionTexture, reflectionUV).rgb, f_color.rgb, 0.5);

    // smooth edges
    f_color.a *= clamp(waterDepth * 1.5, 0.0, 1.0);

    // add fog
    f_color.rgb = mix(f_color.rgb, FOG_COLOR, 1.0 - exp_fog(gl_FragCoord.z / gl_FragCoord.w, FOG_DENSITY));
}