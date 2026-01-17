# Blocks

Tiny Minecraft clone in C and HLSL using the new SDL3 GPU API

### Features

- Procedural world generation
- Asynchronous chunk loading
- Blocks and plants
- Persistent worlds
- Clustered dynamic lighting

### Passes

1. Render opaques to depth texture for shadows
2. Render sky to G-buffer
3. Render opaques to G-buffer
4. Calculate SSAO
5. Blur SSAO
6. Composite G-buffer
7. Render transparents to depth texture (predepth)
8. Render transparents and composite
9. Render block raycast
10. Render UI
11. Blit to swapchain texture
