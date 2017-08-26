#version 450 core

in vec3 triangleDist;
out vec4 f_color;

// http://codeflow.org/entries/2012/aug/02/easy-wireframe-display-with-barycentric-coordinates/
vec4 edge_factor(float lineWidth) 
{
    vec3 d = fwidth(triangleDist) * 10;

    vec3 a3 = smoothstep(vec3(0), vec3(0), d * triangleDist);

    vec2 dist = (gl_FragCoord.xy) / 1280.0; //distance(gl_FragCoord.xy / 2.0, triangleDist.xy);
    float m = min(min(d.x, d.y), d.z);
    return vec4(m * dist.x, m * dist.x, 1, 1);
    //return vec4(dist.x * d.x, dist.y * d.y, dist.y, 1);
}

/*
// returns a wireframed color from a fill, stroke and linewidth
vec3 wireframe(vec3 fill, vec3 stroke, float lineWidth) 
{
    return mix(stroke, fill, edge_factor(lineWidth));
}

// returns a black wireframed color from a fill and linewidth
vec3 wireframe(vec3 color, float lineWidth) 
{
    return wireframe(color, vec3(0, 0, 0), lineWidth);
}

// returns a black wireframed color
vec3 wireframe(vec3 color) 
{
    return wireframe(color, 1.0);
}
*/

void main()
{
    f_color = edge_factor(0.1);
}
