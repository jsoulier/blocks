# Blocks

![](image.png)

Tiny Minecraft clone in C and HLSL using the new SDL3 GPU API

### Features

- Procedural world generation
- Asynchronous chunk loading
- Persistent worlds
- Physics
- Directional shadows
- Clustered dynamic lighting
- Blocks and sprites
- Day and night cycle

### Building

#### Windows

```bash
git clone https://github.com/jsoulier/blocks --recurse-submodules
cd blocks
mkdir build
cd build
cmake ..
cmake --build . --parallel 8 --config Release
cd bin
./blocks.exe
```

#### Linux

```bash
git clone https://github.com/jsoulier/blocks --recurse-submodules
cd blocks
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel 8
cd bin
./blocks
```

#### Shaders

Shaders are precompiled.
To build locally, add [SDL_shadercross](https://github.com/libsdl-org/SDL_shadercross) to your path

### Controls

- `WASDEQ` to move
- `Space` to jump
- `F5` to toggle fly
- `Escape` to unfocus
- `Left Click` to break a block
- `Middle Click` to select a block
- `Right Click` to place a block
- `Scroll` to change blocks
- `F11` to toggle fullscreen
- `LControl` to sprint
- `T` to reset the time of day

### Passes

1. Render opaques to depth texture (for shadows)
2. Render sky and opaques with CPU-baked vertex AO
3. Render transparents to depth texture (for predepth)
4. Render transparents and composite
5. Render raycast
6. Render UI
