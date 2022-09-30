#version 460

layout (std140, binding = 0) uniform UNIFORMS
{
	int GaussN;
	float[64 * 64] Kernel;
} Uniforms;

layout (std140, binding = 2) uniform RESOLUTIUON_UNIFORMS
{
	float TexelWidth;
	float TexelHeight;
} ResolutionUniforms;

layout (binding = 1) uniform sampler2D Depth;

layout (location = 0) in vec2 UV;

layout (location = 0) out float SmoothedDepth;

void main()
{
	float sum = 0;

	for (int i = -Uniforms.GaussN; i <= Uniforms.GaussN; ++i)
	{
		for (int j = -Uniforms.GaussN; j <= Uniforms.GaussN; ++j)
		{
			int index = abs(i) * (Uniforms.GaussN + 1) + abs(j);

			sum +=
				Uniforms.Kernel[index] *
				texture(Depth, UV + vec2(i * ResolutionUniforms.TexelWidth, j * ResolutionUniforms.TexelHeight)).r;
		}
	}

	// SmoothedDepth = sum;
	// SmoothedDepth = 0.0;
	gl_FragDepth = sum;

	// gl_FragDepth = 0.0;
}
