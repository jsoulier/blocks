# Blocks

![](image.png)

Tiny Minecraft clone in C and HLSL using the new SDL3 GPU API

### Features

- Procedural world generation
- Asynchronous chunk loading
- Persistent worlds
- Physics
- Blocks and sprites
- Basic lighting
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

#### Android

Open `android/` in Android Studio and press `Run`.

#### Shaders

Shaders are precompiled.
To build locally, add [SDL_shadercross](https://github.com/libsdl-org/SDL_shadercross) to your path

### Controls

#### Desktop

- `WASDEQ` to move
- `Space` to jump
- `F5` to toggle fly
- `Escape` to unfocus
- `Left Click` to break a block
- `Middle Click` to select a block
- `Right Click` to place a block
- `Scroll` to change the selected block
- `F11` to toggle fullscreen
- `LControl` to sprint
- `T` to reset the time of day

#### Mobile

- Drag the left half of the screen to move
- Drag the right half of the screen to look
- `Blue` to jump
- `Red` to break a block
- `Green` to place a block
- `Grey` to change the selected block
- `Yellow` to sprint
