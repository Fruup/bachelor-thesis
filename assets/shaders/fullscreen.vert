#version 460

layout (std140, binding = 2) uniform UNIFORMS
{
	float TexelWidth;
	float TexelHeight;
} Uniforms;

layout (location = 0) out vec2 UV;
layout (location = 1) out vec2 tl;
layout (location = 2) out vec2 tm;
layout (location = 3) out vec2 tr;
layout (location = 4) out vec2 ml;
layout (location = 5) out vec2 mr;
layout (location = 6) out vec2 bl;
layout (location = 7) out vec2 bm;
layout (location = 8) out vec2 br;

void main()
{
	if (gl_VertexIndex == 0)
		UV = vec2(0, 0);
	else if (gl_VertexIndex == 1)
		UV = vec2(0, 2);
	else if (gl_VertexIndex == 2)
		UV = vec2(2, 0);

	const float w = Uniforms.TexelWidth;
	const float h = Uniforms.TexelHeight;
	// const float w = 0.001;
	// const float h = 0.001;

	gl_Position = vec4(2 * UV - 1, 0, 1);

	tl = UV + vec2(-w, -h);
	tm = UV + vec2( 0, -h);
	tr = UV + vec2(+w, -h);
	ml = UV + vec2(-w,  0);
	mr = UV + vec2(+w,  0);
	bl = UV + vec2(-w, +h);
	bm = UV + vec2( 0, +h);
	br = UV + vec2(+w, +h);
}
