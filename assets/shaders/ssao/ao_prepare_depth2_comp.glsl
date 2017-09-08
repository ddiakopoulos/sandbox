// Based on SSAO in Microsoft's MiniEngine (https://github.com/Microsoft/DirectX-Graphics-Samples/tree/master/MiniEngine)
// Original Copyright (c) 2013-2015 Microsoft (MIT Licence)
// Transliterated to GLSL compute in 2017 by https://github.com/ddiakopoulos

#version 450

uniform sampler2D DS4x;

layout (binding = 0, r32f) writeonly uniform image2D DS8x;
layout (binding = 1, r32f) writeonly uniform image2DArray DS8xAtlas;
layout (binding = 2, r32f) writeonly uniform image2D DS16x;
layout (binding = 3, r32f) writeonly uniform image2DArray DS16xAtlas;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

void main()
{
    float m1 = float(texelFetch(DS4x, ivec2(gl_GlobalInvocationID.xy << uvec2(1)), 0));

    uvec2 st = gl_GlobalInvocationID.xy;
    uvec2 stAtlas = st >> uvec2(2);
    uint stSlice =  ((st.x & 3) | (st.y << 2)) & 15;

    imageStore(DS8x, ivec2(st), vec4(m1));
    imageStore(DS8xAtlas, ivec3(uvec3(stAtlas, stSlice)), vec4(m1));

    if ((gl_LocalInvocationIndex & 011) == 0)
    {
        uvec2 st = gl_GlobalInvocationID.xy >> uvec2(1);
        uvec2 stAtlas = st >> uvec2(2);
        uint stSlice =  ((st.x & 3) | (st.y << 2)) & 15;
        imageStore(DS16x, ivec2(st), vec4(m1));
        imageStore(DS16xAtlas, ivec3(uvec3(stAtlas, stSlice)), vec4(m1));
    }
}
