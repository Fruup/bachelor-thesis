#version 460

layout (location = 0) out vec2 UV;

void main()
{
	if (gl_VertexIndex == 0)
		UV = vec2(0, 0);

	else if (gl_VertexIndex == 1)
		UV = vec2(0, 1);

	else if (gl_VertexIndex == 2)
		UV = vec2(1, 0);

	gl_Position = vec4(4 * UV - 1, 0, 1);
}
