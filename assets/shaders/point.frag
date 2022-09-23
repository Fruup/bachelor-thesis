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

layout (location = 0) out vec4 FragColor;

void main()
{
	// compute spherical offset from UVs
	vec2 r = 2 * Input.UV - vec2(1.0);
	float l = length(r);

	if (l > 1) discard;

	float sphericalOffset = cos(PI_OVER_TWO * l);

	// project view space z coordinate to screen
	vec3 pView = Input.ViewPosition - Uniforms.Radius * vec3(0, 0, sphericalOffset);

	vec4 pScreen = Uniforms.Projection * vec4(pView, 1);
	float zScreen = pScreen.z;

	gl_FragDepth = zScreen;

	FragColor = vec4(vec3(zScreen), 1);
	// FragColor = vec4(vec3(sphericalOffset), 1);
}
