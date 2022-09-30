#version 460

layout (std140, binding = 0) uniform UNIFORMS
{
	mat4 View;
	mat4 Projection;

	float Radius; // world space
} Uniforms;

layout (location = 0) in vec3 a_Position;
layout (location = 1) in vec2 a_UV;

layout (location = 0) out FRAGMENT_IN
{
	vec3 ViewPosition; // view space
	vec2 UV;
} Output;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{
	const vec4 viewPosition = Uniforms.View * vec4(a_Position, 1);

	Output.ViewPosition = viewPosition.xyz;
	Output.UV = a_UV;

	gl_Position = Uniforms.Projection * viewPosition;
}
