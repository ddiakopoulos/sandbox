#version 330

uniform mat4 u_mvpMatrix;
uniform mat4 u_modelViewMatrix;
uniform mat3 u_normalMatrix;

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec3 inNormal;

out vec3 cameraIncident;
out vec3 cameraNormal;

void main()
{
    // Incident ray in camera space
    cameraIncident = normalize(vec3(u_modelViewMatrix * inPosition));
    
    // Normal in camera space
    cameraNormal = normalize(u_normalMatrix * inNormal);

    gl_Position = u_mvpMatrix * inPosition;
}