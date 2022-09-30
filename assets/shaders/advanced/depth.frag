#version 460

#define PI_OVER_TWO (1.57079632679)

layout (std140, binding = 0) uniform UNIFORMS
{
	mat4 View;
	mat4 Projection;

	float Radius; // world space
} Uniforms;

layout (location = 0) in FRAGMENT_IN
{
	vec3 ViewPosition; // view space
	vec2 UV;
} Input;

void main()
{
	// compute spherical offset from UVs
	const float l = length(2 * Input.UV - 1);
	if (l > 1) discard;

	const float sphericalOffset = cos(PI_OVER_TWO * l);

	// project view space z coordinate to screen
	const vec3 pView = Input.ViewPosition - Uniforms.Radius * vec3(0, 0, sphericalOffset);

	const vec4 pScreen = Uniforms.Projection * vec4(pView, 1);
	const float zScreen = pScreen.z / pScreen.w;

	gl_FragDepth = zScreen;
}
