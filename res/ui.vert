#version 450

layout(location = 0) in vec2 position; // Vertex position

// layout(set = 1, binding = 0) uniform UniformBufferObject {
//     float aspectRatio; // Aspect ratio of the screen
// };

void main()
{
    // Adjust the position for the aspect ratio
    vec2 adjustedPosition = vec2(position.x /* * aspectRatio */, position.y);

    // Convert to normalized device coordinates (NDC)
    gl_Position = vec4(adjustedPosition, 0.0, 1.0);
}
