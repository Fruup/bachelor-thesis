#version 460

layout (std140, binding = 2) uniform UNIFORMS
{
	float TexelWidth;
	float TexelHeight;
} ResolutionUniforms;

layout (binding = 0) uniform sampler2D Image;

layout (location = 0) in vec2 UV;

layout (location = 0) out vec4 Color;

void main()
{
	Color = vec4(vec3(1-texture(Image, UV).r), 1);
	// Color = vec4(texture(Image, UV).r, 0, 0, 1);
	// Color = vec4(1,0,0,1);
}
