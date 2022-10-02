#version 460

layout (std140, binding = 0) uniform UNIFORMS
{
	mat4 ProjectionView;
} Uniforms;

layout (location = 0) in FRAGMENT_IN
{
	vec3 Color;
} Input;

layout (location = 0) out vec4 Color;

void main()
{
	Color = vec4(Input.Color, 1);
}
