#version 330
layout(location = 0) in vec3 vertexModelspace;
// Output data - will be interpolated for each fragment.
out vec2 texCoord;
void main()
{
	gl_Position = vec4(vertexModelspace, 1);
	texCoord = (vertexModelspace.xy + vec2(1,1)) / 2.0;
}