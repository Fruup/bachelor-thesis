#version 460

#define MAX_KERNEL_N 32

layout (std140, binding = 0) uniform UNIFORMS2
{
	int GaussN;
	vec4[MAX_KERNEL_N * MAX_KERNEL_N / 4] Kernel;
} Uniforms;

layout (std140, binding = 2) uniform UNIFORMS
{
	float TexelWidth;
	float TexelHeight;
} ResolutionUniforms;

layout (binding = 1) uniform sampler2D Depth;

layout (location = 0) in vec2 UV;

layout (location = 0) out float SmoothedDepth;

float getKernelValue(int index)
{
	return Uniforms.Kernel[index / 4][index % 4];
}

void main()
{
	const int N = Uniforms.GaussN;
	const vec2 res = vec2(ResolutionUniforms.TexelWidth, ResolutionUniforms.TexelHeight);
	float sum = 0;

	for (int i = -N; i <= N; ++i)
	{
		for (int j = -N; j <= N; ++j)
		{
			int index = abs(i) * (Uniforms.GaussN + 1) + abs(j);

			sum +=
				getKernelValue(index) *
				texture(Depth, UV + vec2(i, j) * res).r;
		}
	}

	SmoothedDepth = sum;
}
