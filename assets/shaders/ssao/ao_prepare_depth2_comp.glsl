#version 430

uniform sampler2D DS4x;

writeonly uniform image2D DS8x;
writeonly uniform image2DArray DS8xAtlas;
writeonly uniform image2D DS16x;
writeonly uniform image2DArray DS16xAtlas;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

void main()
{
    float m1 = float(texelFetch(DS4x, ivec2(gl_GlobalInvocationID.xy << uvec2(1)), 0));

    uvec2 st = gl_GlobalInvocationID.xy;
    uvec2 stAtlas = st >> uvec2(2);
    uint stSlice = (st.x & 3u) | ((st.y & 3u) << 2u);

    imageStore(DS8x, ivec2(st), vec4(m1));
    imageStore(DS8xAtlas, ivec3(uvec3(stAtlas, stSlice)), vec4(m1));

    if ((gl_LocalInvocationIndex & 11u) == 0u)
    {
        uvec2 st = gl_GlobalInvocationID.xy >> uvec2(1);
        uvec2 stAtlas = st >> uvec2(2);
        uint stSlice = (st.x & 3u) | ((st.y & 3u) << 2u);
        imageStore(DS16x, ivec2(st), vec4(m1));
        imageStore(DS16xAtlas, ivec3(uvec3(stAtlas, stSlice)), vec4(m1));
    }
}
