#version 460

// Input: depth buffer, view matrix (inverted and transposed?)
// Output: per pixel normals in world space

// with help from
// https://wickedengine.net/2019/09/22/improved-normal-reconstruction-from-depth/

// consider also
// https://atyuwen.github.io/posts/normal-reconstruction/

layout (std140, binding = 0) uniform UNIFORMS
{
	mat4 ViewProjectionInv;
	vec2 Resolution;
} Uniforms;

layout (location = 0) in INPUT
{
	vec3 ClipPosition;
} Input;

vec3 positionFromDepth()
{
	vec4 world = Uniforms.ViewProjectionInv * vec4(Input.ClipPosition, 1);

	return world.xyz / world.w;
}

layout (location = 3) out vec4 Normal;

void main()
{
	vec3 p = positionFromDepth();
	Normal.xzy = normalize(cross(dFdx(p), dFdy(p)));
}
