#version 450

layout(location = 0) in vec2 position;  // Input vertex positions
layout(location = 0) out vec2 uv;        // Output UV coordinates

// Define UV offsets for the texture atlas face at (2, 0)
const vec2 uvOffsets[6] = vec2[](
    vec2(0.0, 0.0),   // Vertex 0: bottom-left (uMin, vMin)
    vec2(1.0, 0.0),  // Vertex 1: bottom-right (uMax, vMin)
    vec2(0.0, 1.0), // Vertex 2: top-left (uMin, vMax)
    vec2(1.0, 1.0),// Vertex 3: top-right (uMax, vMax)
    vec2(1.0, 0.0),   // Vertex 4: bottom-right (uMax, vMin)
    vec2(0.0, 1.0)  // Vertex 5: top-left (uMin, vMax)
);

void main()
{
    // Set the position in clip space
    gl_Position = vec4(position, 0.0, 1.0);
    
    // Assign UV coordinates based on the vertex index
    uv = uvOffsets[gl_VertexIndex]; // Use gl_VertexID to access UV offset
}


        // const float face = 16.0;
        // const float size = 256.0;
        // const float c = 2;
        // const float d = 0;
        // const float e = c * face / size;
        // const float f = d * face / size;
        // // color = texture(atlas, vec2(e, f));
