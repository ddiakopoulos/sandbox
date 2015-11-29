#version 330

out vec4 f_color;

in vec3 vPosition;
in vec3 norm;
in vec3 p;

uniform mat4 u_modelView;
uniform mat3 u_modelMatrixIT;
uniform vec3 u_lightPosition;
uniform vec3 u_eyePosition;
uniform vec4 u_clipPlane;

void main(void) 
{
    float clipPos = dot (vPosition, u_clipPlane.xyz) + u_clipPlane.w;
    //if (clipPos < 0.0) discard;

    vec3 p0 = dFdx(p);
    vec3 p1 = dFdy(p);
    vec3 n = u_modelMatrixIT * normalize(cross(p0, p1));

    vec3 surfaceColor = vec3(1.0, 0.5, 1.0);
    float ambientIntensity = 0.1;

    vec3 surfacePos = (u_modelView * vec4(p, 0.0)).xyz;
    vec3 surfaceToLight = normalize(u_lightPosition - surfacePos);

    vec3 ambient = ambientIntensity * surfaceColor;
    float diffuseCoefficient = max(0.0, dot(n, surfaceToLight));
    vec3 diffuse = diffuseCoefficient * surfaceColor;

    vec3 lightFactor = ambient + diffuse;

    f_color = vec4(lightFactor, 1.0);
}