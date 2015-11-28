#version 330

out vec4 f_color;

in float vOffset;
in vec3 vPosition;

uniform mat4 u_modelMatrix;
uniform mat3 u_modelMatrixIT;
uniform vec3 u_lightPosition;

#define FOG_DENSITY 0.05
#define FOG_COLOR vec3(1.0)

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

    vec3 surfaceColor = vec3(0.4, 0.45, 0.425);
    float ambientIntensity = 0.05;

    vec3 surfaceToLight = normalize(u_modelMatrix * vec4(u_lightPosition, 0.0)).xyz;
    vec3 surfaceToCamera = normalize(-u_modelMatrix * vec4(vPosition, 0.0)).xyz;

    vec3 ambient = ambientIntensity * surfaceColor;
    float diffuseCoefficient = max(0.0, dot(n, surfaceToLight));
    vec3 diffuse = diffuseCoefficient * surfaceColor;

    vec3 lightFactor = ambient + diffuse;

    f_color = vec4(lightFactor, 1.0);
    f_color.rgb = mix(f_color.rgb, FOG_COLOR, 1.0 - exp_fog(gl_FragCoord.z / gl_FragCoord.w, FOG_DENSITY));
}