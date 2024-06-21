#version 450

// Input
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

// Output
layout(location = 0) out vec4 outColor;

// Shader function
void main() {
	// Pass through
    gl_Position = vec4(inPosition, 1.0);
    outColor = inColor;
}