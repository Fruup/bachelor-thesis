#version 460

const vec4 DiffuseColor = vec4(0, 1, 0, 1);
const vec4 SpecularColor = vec4(1);
const float SpecularExponent = 5;

layout (std140, binding = 0) uniform UNIFORMS
{
	vec3 CameraPosition;
	vec3 CameraDirection;

	vec3 LightDirection;
} Uniforms;

//layout (location = 0) in vec3 Position;
//layout (location = 1) in vec3 Normal;

layout (input_attachment_index = 0, set = 0, binding = 1) uniform subpassInput Position;
layout (input_attachment_index = 1, set = 0, binding = 2) uniform subpassInput Normal;

//layout (input_attachment_index = 1, set = 0, binding = 3) uniform subpassInput Depth;

layout (location = 0) out vec4 Color;

void main()
{
	// const float diffuse = -dot(Uniforms.LightDirection, Normal);

	// const float specular = dot(
	// 	normalize(Uniforms.CameraPosition - Position),
	// 	reflect(Uniforms.LightDirection, Normal));

	// Color =
	// 	max(0, diffuse) * DiffuseColor +
	// 	pow(max(0, specular), SpecularExponent) * SpecularColor;
	Color = vec4(1, 0, 0, 1);
	// Color = vec4(subpassLoad(Position).xyz, 1);
	//Color = vec4(vec3(subpassLoad(Depth).r), 1);
}
