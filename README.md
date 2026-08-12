# Blocks

Tiny Minecraft clone in C and HLSL using the new SDL3 GPU API

![](doc/image3.png)
*Running on Desktop*

![](doc/image1.jpg)
![](doc/image4.jpg)
*Running on a Samsung Galaxy A17*

### Features

- Desktop and Android support
- Keyboard and mouse, gamepad, and touch controls
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

Open `android/` in Android Studio and press `Run`

#### Shaders

Shaders are precompiled.
To build locally, add [SDL_shadercross](https://github.com/libsdl-org/SDL_shadercross) to your path

### Controls

#### Keyboard and Mouse

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

#### Touch

- Left side of the screen to move
- Right side of the screen to look

#### Gamepad

- `Left Stick` to move
- `Right Stick` to look
- `X` to jump or fly up
- `Circle` to fly down
- `Triangle` to toggle fly
- `R2` to break a block
- `R3` to select a block
- `L2` to place a block
- `L1/R1` to change blocks
- `L3` to sprint 
- `Square` to reset the time of day
