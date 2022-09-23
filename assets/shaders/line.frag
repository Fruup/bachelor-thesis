#version 450

// input
layout(location = 0) in vec4 FragColor;

// output
layout(location = 0) out vec4 frag;

void main()
{
    frag = FragColor;
}
