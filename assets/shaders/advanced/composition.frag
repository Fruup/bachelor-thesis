#version 460

const vec4 DiffuseColor = vec4(120, 185, 255, 255) / 255;
// const vec4 DiffuseColor = vec4(1);
const vec4 SpecularColor = 0.5*vec4(1, 1, 1, 1);
const vec4 AmbientColor = vec4(vec3(0.05), 0);
const float SpecularExponent = 1000;

layout (std140, binding = 0) uniform UNIFORMS
{
	mat4 InvProjection;
	mat4 InvProjectionView;

	vec3 CameraPosition;
	vec3 CameraDirection;

	vec3 LightDirection;
} Uniforms;

layout (binding = 1) uniform sampler2D Positions;
layout (binding = 3) uniform sampler2D SmoothedDepth;
layout (binding = 4) uniform sampler2D ObjectNormals;
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

vec4 sampleFloor(vec3 a, vec3 r)
{
	const float FLOOR_HEIGHT = -1.0f;
	vec3 b = a + r * (FLOOR_HEIGHT - a.y) / r.y;

	// 0 or 1
	const vec2 f = step(mod(b.xz, vec2(2)), vec2(1));

	return vec4(vec3(0.25 + (f.x + f.y) / 4), 0.5);
}

vec3 position(vec2 uv) { return texture(Positions, uv).xyz; }

vec3 smoothedPosition(vec2 uv)
{
	vec4 screenH =
		Uniforms.InvProjection *
		vec4(2 * uv - 1, texture(SmoothedDepth, uv).r, 1);

	return screenH.xyz / screenH.w;
}

vec3 viewRay()
{
	vec4 worldH =
		Uniforms.InvProjectionView *
		vec4(2 * UV - 1, 1, 1);

	return worldH.xyz / worldH.w;
}

// float depth(vec2 uv) { return texture(Positions, uv).z; }

void main()
{

	// Color = vec4(texture(Positions, UV).xyz, 1);
	// return;

	// if (texture(SmoothedDepth, UV).r > 0.99)
	// 	discard;

	const vec4 PositionsSampled = texture(Positions, UV);

	if (PositionsSampled.w == 0)
	{
		Color = .75 * sampleFloor(Uniforms.CameraPosition, viewRay());
		return;
	}

#if 0
	vec3 dx =
		+1 * smoothedPosition(tr)
		+2 * smoothedPosition(mr)
		+1 * smoothedPosition(br)
		-1 * smoothedPosition(tl)
		-2 * smoothedPosition(ml)
		-1 * smoothedPosition(bl);
	vec3 dy =
		+1 * smoothedPosition(bl)
		+2 * smoothedPosition(bm)
		+1 * smoothedPosition(br)
		-1 * smoothedPosition(tl)
		-2 * smoothedPosition(tm)
		-1 * smoothedPosition(tr);

	const vec3 screenNormal = normalize(cross(dx, dy));
#endif

	const vec3 world = PositionsSampled.xyz;

	const vec3 objectNormal = texture(ObjectNormals, UV).xyz;
	const vec3 normal = objectNormal;

	// const vec3 normal = normalize(screenNormal + objectNormal);

	// Color = vec4(normal, 1);
	// return;

	const vec3 refracted = refract(normalize(viewRay()), normal, 1.1);
	const vec4 floorColor = sampleFloor(world, refracted);

	float f = -dot(Uniforms.CameraDirection, normal);

	Color = f * floorColor + (0.15 + 1 - f * f) * DiffuseColor;
	return;

#if 1
	// const vec3 light = Uniforms.LightDirection;
	const vec3 light = Uniforms.CameraDirection;
	// const vec3 light = vec3(0, 0, 1);

	const float diffuse = max(-dot(normal, light), 0);
	const float specular = pow(max(dot(light, normalize(reflect(normal, Uniforms.CameraPosition - world))), 0), SpecularExponent);

	Color =
		+ DiffuseColor * diffuse
		+ SpecularColor * specular
		+ AmbientColor
		;
#endif
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

#if 0
	const mat3 K = (1/8) * mat3(
		vec3(-1, 0, +1),
		vec3(-2, 0, +2),
		vec3(-1, 0, +1)
	);

	const float dzdx = (1 / delta) * (
		K[0][0] * depth(tl) + K[1][0] * depth(tm) + K[2][0] * depth(tr) +
		K[0][1] * depth(ml) + K[1][1] * depth(UV) + K[2][1] * depth(mr) +
		K[0][2] * depth(bl) + K[1][2] * depth(bm) + K[2][2] * depth(br)
	);

	const float dzdy = (1 / delta) * (
		K[0][0] * depth(tl) + K[0][1] * depth(tm) + K[0][2] * depth(tr) +
		K[1][0] * depth(ml) + K[1][1] * depth(UV) + K[1][2] * depth(mr) +
		K[2][0] * depth(bl) + K[2][1] * depth(bm) + K[2][2] * depth(br)
	);
#endif
