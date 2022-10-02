#version 460

layout (std140, binding = 0) uniform UNIFORMS
{
	mat4 ProjectionView;
	float Aspect;
} Uniforms;

layout (location = 0) in vec3 a_Position;

layout (location = 0) out FRAGMENT_IN
{
	vec3 Color;
} Output;

void main()
{
	if (gl_VertexIndex < 2)      Output.Color = vec3(1, 0, 0);
	else if (gl_VertexIndex < 4) Output.Color = vec3(0, 1, 0);
	else                         Output.Color = vec3(0, 0, 1);

	vec4 screenH = Uniforms.ProjectionView * vec4(a_Position, 1);
	// gl_Position = screenH / screenH.w - vec4(Uniforms.Aspect / 2, 0.8, 0, 0);
	gl_Position = screenH;
}
