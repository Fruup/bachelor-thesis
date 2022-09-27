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

layout (binding = 1) uniform sampler2D Positions;
// layout (binding = 2) uniform sampler2D Normals;
// layout (binding = 3) uniform sampler2D Depth;

layout (location = 0) in vec2 UV;
layout (location = 1) in vec2 tl;
layout (location = 2) in vec2 tm;
layout (location = 3) in vec2 tr;
layout (location = 4) in vec2 ml;
layout (location = 5) in vec2 mr;
layout (location = 6) in vec2 bl;
layout (location = 7) in vec2 bm;
layout (location = 8) in vec2 br;

layout (location = 0) out vec4 Color;

const mat3 K = (1/8) * mat3(
	vec3(-1, 0, +1),
	vec3(-2, 0, +2),
	vec3(-1, 0, +1)
);

float position(vec2 uv) { return length(texture(Positions, uv).xyz); }

void main()
{
	// const float f =
	// 	K[0][0] * position(tl) + K[1][0] * position(tm) + K[2][0] * position(tr) +
	// 	K[0][1] * position(ml) + K[1][1] * position(UV) + K[2][1] * position(mr) +
	// 	K[0][2] * position(bl) + K[1][2] * position(bm) + K[2][2] * position(br);

	const float f = position(UV);

	Color = vec4(abs(texture(Positions, UV).xyz), 1);
	// Color = vec4(Positions.xyz, 1);
	// Color = vec4(UV, 0, 1);
	// Color = vec4(1, 0, 0, 1);
}

#if 0
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
#endif
