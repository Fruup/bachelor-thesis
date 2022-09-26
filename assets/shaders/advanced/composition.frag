#version 460

const vec4 DiffuseColor = vec4(1, 1, 1, 1);
const vec4 SpecularColor = vec4(1, 0, 0, 1);
const float SpecularExponent = 5;

layout (std140, binding = 0) uniform UNIFORMS
{
	mat4 InvProjection;

	vec3 CameraPosition;
	vec3 CameraDirection;

	vec3 LightDirection;
} Uniforms;

layout (location = 0) in vec2 UV;

layout (location = 0) out vec4 Color;

layout (location = 1) out vec4 Positions;
layout (location = 2) out vec4 Normals;

void main()
{
	// TODO: check if depth buffer is 1 here
	//if (ClipSpace.z == 1)
	//	discard;

	// TODO: This is in view space. Convert to world space.
	vec3 world = Positions.xyz;
	float z = world.z;

	vec3 _n = normalize(cross(
		dFdx(world),
		dFdy(world)
	));

	vec3 normal = vec3(_n.x, _n.y, _n.z);

	// Color = vec4(normal, 1);
	// return;

	const float diffuse = dot(Uniforms.LightDirection, normal);

	const float specular = -dot(
		normalize(Uniforms.CameraPosition - world),
		reflect(Uniforms.LightDirection, normal));

	Color =
		max(0, diffuse) * DiffuseColor 
		;

	//Color = vec4(1, 0, 0, 1);
	//Color = vec4(ClipSpace.z, 0, 0, 1);

	// Color = vec4(1, 0, 0, 1);
	// Color = vec4(subpassLoad(Position).xyz, 1);
	// Color = vec4(vec3(subpassLoad(Depth).r), 1);
}
