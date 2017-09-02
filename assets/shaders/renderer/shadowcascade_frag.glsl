#include "renderer_common.glsl"

uniform float u_cascadeNear[NUM_CASCADES];
uniform float u_cascadeFar[NUM_CASCADES];

in float g_layer;
in vec3 vs_position;
out float f_depth;

void main() 
{
    int layer = int(g_layer);
    float linearDepth = (-vs_position.z - u_cascadeNear[layer] ) / ( u_cascadeFar[layer] - u_cascadeNear[layer]);
    f_depth = linearDepth; 
}