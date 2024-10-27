#version 450

layout(location = 0) out vec4 color; // Output color

layout(set = 3, binding = 0) uniform window_t {
     vec2 size;
} window;

void main()
{
     float crosshairSize = 0.02f;
    // Convert fragment coordinates to normalized coordinates
    vec2 normalizedCoords = (gl_FragCoord.xy / window.size) * 2.0 - 1.0; // Adjust based on viewport size
    normalizedCoords.x *= window.size.x / window.size.y;

    // Check if the fragment is within the crosshair area
    bool isCrosshair = (abs(normalizedCoords.x) < crosshairSize && abs(normalizedCoords.y) < 0.005) || // Horizontal line
                       (abs(normalizedCoords.y) < crosshairSize && abs(normalizedCoords.x) < 0.005); // Vertical line

    // If not part of the crosshair, discard the fragment
    if (!isCrosshair) {
        discard; // Discard fragments outside the crosshair
    }

    // Set color for the crosshair
    color = vec4(1.0, 0.0, 0.0, 1.0); // Red color for the crosshair
}
