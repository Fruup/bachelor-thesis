#version 450

// uniforms
layout(binding = 0) uniform UniformBufferObject
{
    mat4 ProjectionView;
    vec2 CursorWorldPosition;
} Uniforms;

// input
layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec4 a_Color;

// output
layout(location = 0) out vec4 FragColor;

void main()
{
    gl_Position = Uniforms.ProjectionView * vec4(a_Position, 0.0, 1.0);

    FragColor = a_Color;
}
