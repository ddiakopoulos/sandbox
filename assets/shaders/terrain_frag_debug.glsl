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
uniform vec3 u_surfaceColor;

bool isOnPlane(vec3 point, vec3 normal, float elevation) 
{
    if (elevation <= 0.0f) 
        return false;
    else if (normal.y <= 0.0f) 
        return point.y <= elevation;
    else 
        return point.y >= elevation;
}

bool isClipped(vec4 worldPosition) 
{
    return isOnPlane(worldPosition.xyz, u_clipPlane.xyz, u_clipPlane.w);
}

void discardIfClipped(vec4 worldPosition) 
{
    if (isClipped(worldPosition)) discard;
}

void main(void) 
{
    discardIfClipped(vec4(vPosition, 1.0));

    vec3 p0 = dFdx(p);
    vec3 p1 = dFdy(p);
    vec3 n = u_modelMatrixIT * normalize(cross(p0, p1));

    vec3 surfaceColor = u_surfaceColor;
    float ambientIntensity = 0.33;

    vec3 surfacePos = (u_modelView * vec4(p, 0.0)).xyz;
    vec3 surfaceToLight = normalize(u_lightPosition - surfacePos);

    vec3 ambient = ambientIntensity * surfaceColor;
    float diffuseCoefficient = max(0.0, dot(n, surfaceToLight));
    vec3 diffuse = diffuseCoefficient * surfaceColor;

    vec3 lightFactor = ambient + diffuse;

    f_color = vec4(lightFactor, 1.0);
}