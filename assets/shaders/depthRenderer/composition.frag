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
layout (location = 1) in vec2 tl;
layout (location = 2) in vec2 tm;
layout (location = 3) in vec2 tr;
layout (location = 4) in vec2 ml;
layout (location = 5) in vec2 mr;
layout (location = 6) in vec2 bl;
layout (location = 7) in vec2 bm;
layout (location = 8) in vec2 br;

layout (input_attachment_index = 0, set = 0, binding = 1) uniform subpassInput Depth;

layout (location = 0) out vec4 Color;

void main()
{
	vec3 ClipSpace = vec3(2 * UV - 1, subpassLoad(Depth)[0]);

	if (ClipSpace.z == 1)
		discard;

	vec4 worldH = Uniforms.InvProjection * vec4(ClipSpace, 1);
	vec3 world = worldH.xyz / worldH.w;
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
