#version 420

uniform float u_cascadeNear[4];
uniform float u_cascadeFar[4];

in float g_layer;
in vec3 vs_position;
out float f_depth;

void main() 
{
    int layer = int(g_layer);
    float linearDepth = (-vs_position.z - u_cascadeNear[layer] ) / ( u_cascadeFar[layer] - u_cascadeNear[layer]);
    f_depth = linearDepth; 
}