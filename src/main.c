#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include "camera.h"
#include "noise.h"
#include "world.h"

static SDL_Window* window;
static SDL_GPUDevice* device;
static SDL_GPUTexture* color_texture;
static SDL_GPUTexture* depth_texture;
static SDL_GPUGraphicsPipeline* world_pipeline;
static SDL_GPUGraphicsPipeline* ui_pipeline;
static SDL_GPUBuffer* ui_vbo;
static SDL_GPUTexture* atlas_texture;
static SDL_GPUSampler* atlas_sampler;
static SDL_Surface* atlas_surface;
static uint32_t width;
static uint32_t height;
static camera_t camera;

////////////////////////////////////////////////////////////////////////////////
// Helpers /////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static SDL_GPUShader* load_shader(
    const char* file,
    const int uniforms,
    const int samplers)
{
    assert(device);
    assert(file);
    SDL_GPUShaderCreateInfo info = {0};
    void* code = SDL_LoadFile(file, &info.code_size);
    if (!code) {
        SDL_Log("Failed to load %s shader: %s", file, SDL_GetError());
        return NULL;
    }
    info.code = code;
    if (strstr(file, ".vert")) {
        info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
    } else {
        info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    }
    info.format = SDL_GPU_SHADERFORMAT_SPIRV;
    info.entrypoint = "main";
    info.num_uniform_buffers = uniforms;
    info.num_samplers = samplers;
    SDL_GPUShader* shader = SDL_CreateGPUShader(device, &info);
    SDL_free(code);
    if (!shader) {
        SDL_Log("Failed to create %s shader: %s", file, SDL_GetError());
        return NULL;
    }
    return shader;
}

static SDL_Surface* load_bmp(const char* file)
{
    SDL_Surface* argb32 = SDL_LoadBMP(file);
    if (!argb32) {
        SDL_Log("Failed to load %s: %s", file, SDL_GetError());
        return NULL;
    }
    SDL_Surface* rgba32 = SDL_ConvertSurface(argb32, SDL_PIXELFORMAT_RGBA32);
    if (!rgba32) {
        SDL_Log("Failed to convert %s: %s", file, SDL_GetError());
        return NULL;
    }
    SDL_DestroySurface(argb32);
    return rgba32;
}

////////////////////////////////////////////////////////////////////////////////
// Atlas Implementation ////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static void load_atlas()
{
    atlas_surface = load_bmp("atlas.bmp");
    if (!atlas_surface) {
        return;
    }
    const int w = atlas_surface->w;
    const int h = atlas_surface->h;
    SDL_GPUTextureCreateInfo tci = {0};
    tci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    tci.type = SDL_GPU_TEXTURETYPE_2D;
    tci.layer_count_or_depth = 1;
    tci.num_levels = 1;
    tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tci.width = w;
    tci.height = h;
    atlas_texture = SDL_CreateGPUTexture(device, &tci);
    if (!atlas_texture) {
        SDL_Log("Failed to create atlas texture: %s", SDL_GetError());
        return;
    }
    SDL_GPUTransferBufferCreateInfo tbci = {0};
    tbci.size = w * h * 4;
    tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    SDL_GPUTransferBuffer* buffer = SDL_CreateGPUTransferBuffer(device, &tbci);
    if (!buffer) {
        SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
        return;
    }
    void* data = SDL_MapGPUTransferBuffer(device, buffer, 0);
    if (!data) {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        return;
    }
    memcpy(data, atlas_surface->pixels, w * h * 4);
    SDL_UnmapGPUTransferBuffer(device, buffer);
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
    if (!commands) {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return;
    }
    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(commands);
    if (!pass) {
        SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
        return;
    }
    SDL_GPUTextureTransferInfo tti = {0};
    tti.transfer_buffer = buffer;
    SDL_GPUTextureRegion region = {0};
    region.texture = atlas_texture;
    region.w = w;
    region.h = h;
    region.d = 1;
    SDL_UploadToGPUTexture(pass, &tti, &region, 0);
    SDL_EndGPUCopyPass(pass);
    SDL_SubmitGPUCommandBuffer(commands);
    SDL_ReleaseGPUTransferBuffer(device, buffer);
    SDL_GPUSamplerCreateInfo sci = {0};
    sci.min_filter = SDL_GPU_FILTER_NEAREST;
    sci.mag_filter = SDL_GPU_FILTER_NEAREST;
    sci.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sci.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sci.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sci.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    atlas_sampler = SDL_CreateGPUSampler(device, &sci);
    if (!atlas_sampler) {
        SDL_Log("Failed to create atlas sampler: %s", SDL_GetError());
        return;
    }
}

SDL_Surface* get_atlas_icon(const block_t block)
{
    if (!atlas_surface) {
        return NULL;
    }
    const int w = 16;
    const int h = 16;
    SDL_Surface* icon = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
    if (!icon) {
        SDL_Log("Failed to create icon surface: %s", SDL_GetError());
        return NULL;
    }
    SDL_Rect src;
    src.x = blocks[block][0][0] * w;
    src.y = blocks[block][0][1] * h;
    src.w = w;
    src.h = h;
    SDL_Rect dst;
    dst.x = 0;
    dst.y = 0;
    dst.w = w;
    dst.h = h;
    if (!SDL_BlitSurface(atlas_surface, &src, icon, &dst)) {
        SDL_Log("Failed to blit icon surface: %s", SDL_GetError());
        SDL_DestroySurface(icon);
        return NULL;
    }
    return icon;
}

////////////////////////////////////////////////////////////////////////////////
// World Implementation ////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static void load_world_pipeline()
{
    SDL_GPUGraphicsPipelineCreateInfo info = {
        .vertex_shader = load_shader("world.vert", 3, 0),
        .fragment_shader = load_shader("world.frag", 0, 1),
        .target_info = {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[]) {{
                .format = SDL_GetGPUSwapchainTextureFormat(device, window)
            }},
            .has_depth_stencil_target = 1,
            .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        },
        .vertex_input_state = {
            .num_vertex_attributes = 1,
            .vertex_attributes = (SDL_GPUVertexAttribute[]) {{
                .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT,
            }},
            .num_vertex_buffers = 1,
            .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]) {{
                .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                .pitch = 4,
            }},
        },
        .depth_stencil_state = {
            .enable_depth_test = 1,
            .enable_depth_write = 1,
            .compare_op = SDL_GPU_COMPAREOP_LESS,
        },
        .rasterizer_state = {
            .cull_mode = SDL_GPU_CULLMODE_BACK,
            .front_face = SDL_GPU_FRONTFACE_CLOCKWISE,
            .fill_mode = SDL_GPU_FILLMODE_FILL,
        },
    };
    world_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    if (!world_pipeline) {
        SDL_Log("Failed to create world pipeline: %s", SDL_GetError());
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
}

static void draw_world(SDL_GPUCommandBuffer* commands)
{
    SDL_GPUColorTargetInfo cti = {0};
    cti.clear_color = (SDL_FColor) {1.0f, 0.0f, 1.0f, 1.0f};
    cti.load_op = SDL_GPU_LOADOP_CLEAR;
    cti.store_op = SDL_GPU_STOREOP_STORE;
    cti.texture = color_texture;
    SDL_GPUDepthStencilTargetInfo dsti = {0};
    dsti.clear_depth = 1;
    dsti.load_op = SDL_GPU_LOADOP_CLEAR;
    dsti.texture = depth_texture;
    dsti.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &cti, 1, &dsti);
    if (!pass) {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    float scale[2];
    scale[0] = 16.0f / atlas_surface->w;
    scale[1] = 16.0f / atlas_surface->h;
    SDL_GPUTextureSamplerBinding tsb = {0};
    tsb.sampler = atlas_sampler;
    tsb.texture = atlas_texture;
    SDL_BindGPUGraphicsPipeline(pass, world_pipeline);
    SDL_PushGPUVertexUniformData(commands, 0, camera.matrix, 64);
    SDL_PushGPUVertexUniformData(commands, 2, scale, sizeof(scale));
    SDL_BindGPUFragmentSamplers(pass, 0, &tsb, 1);
    world_render(&camera, commands, pass);
    SDL_EndGPURenderPass(pass);
}

////////////////////////////////////////////////////////////////////////////////
// UI Implementation ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static void load_ui_pipeline()
{
    SDL_GPUGraphicsPipelineCreateInfo info = {
        .vertex_shader = load_shader("ui.vert", 0, 0),
        .fragment_shader = load_shader("ui.frag", 0, 0),
        .target_info = {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[]) {{
                .format = SDL_GetGPUSwapchainTextureFormat(device, window)
            }},
        },
        .vertex_input_state = {
            .num_vertex_attributes = 1,
            .vertex_attributes = (SDL_GPUVertexAttribute[]) {{
                .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
            }},
            .num_vertex_buffers = 1,
            .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]) {{
                .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                .pitch = 12,
            }},
        },
    };
    ui_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    if (!ui_pipeline) {
        SDL_Log("Failed to create ui pipeline: %s", SDL_GetError());
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
}

static void create_ui_vbo()
{
    const float vertices[][2] = {
        { 0, 0, },
        { 1, 0, },
        { 0, 1, },
        { 1, 1, },
        { 1, 0, },
        { 0, 1, },
    };
    SDL_GPUBufferCreateInfo bci = {0};
    bci.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    bci.size = sizeof(vertices);
    ui_vbo = SDL_CreateGPUBuffer(device, &bci);
    if (!ui_vbo) {
        SDL_Log("Failed to create vertex buffer: %s", SDL_GetError());
        return;
    }
    SDL_GPUTransferBufferCreateInfo tbci = {0};
    tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbci.size = sizeof(vertices);
    SDL_GPUTransferBuffer* tbo = SDL_CreateGPUTransferBuffer(device, &tbci);
    if (!tbo) {
        SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
        return;
    }
    void* data = SDL_MapGPUTransferBuffer(device, tbo, 0);
    if (!data) {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        return;
    }
    memcpy(data, vertices, sizeof(vertices));
    SDL_UnmapGPUTransferBuffer(device, tbo);
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
    if (!commands) {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return;
    }
    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(commands);
    if (!pass) {
        SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
        return;
    } 
    SDL_GPUTransferBufferLocation location = {0};
    location.transfer_buffer = tbo;
    SDL_GPUBufferRegion region = {0};
    region.size = sizeof(vertices);
    region.buffer = ui_vbo;
    SDL_UploadToGPUBuffer(pass, &location, &region, 1);
    SDL_EndGPUCopyPass(pass);
    SDL_SubmitGPUCommandBuffer(commands); 
    SDL_ReleaseGPUTransferBuffer(device, tbo);
}

static void draw_ui(SDL_GPUCommandBuffer* commands)
{
    SDL_GPUColorTargetInfo cti = {0};
    cti.load_op = SDL_GPU_LOADOP_LOAD;
    cti.store_op = SDL_GPU_STOREOP_STORE;
    cti.texture = color_texture;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &cti, 0, NULL);
    if (!pass) {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    SDL_BindGPUGraphicsPipeline(pass, ui_pipeline);
    SDL_GPUBufferBinding bb = {0};
    bb.buffer = ui_vbo;
    SDL_BindGPUVertexBuffers(pass, 0, &bb, 1);
    SDL_DrawGPUPrimitives(pass, 6, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
}

////////////////////////////////////////////////////////////////////////////////
// Main Render /////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static void draw()
{
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
    if (!commands) {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return;
    }
    uint32_t w;
    uint32_t h;
    if (!SDL_AcquireGPUSwapchainTexture(commands, window, &color_texture, &w, &h)) {
        SDL_Log("Failed to aqcuire swapchain image: %s", SDL_GetError());
        return;
    }
    if (width != w || height != h) {
        if (depth_texture) {
            SDL_ReleaseGPUTexture(device, depth_texture);
            depth_texture = NULL;
        }
        SDL_GPUTextureCreateInfo tci = {0};
        tci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        tci.type = SDL_GPU_TEXTURETYPE_2D;
        tci.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        tci.width = w;
        tci.height = h;
        tci.layer_count_or_depth = 1;
        tci.num_levels = 1;
        depth_texture = SDL_CreateGPUTexture(device, &tci);
        if (!depth_texture) {
            SDL_Log("Failed to create depth texture: %s", SDL_GetError());
            return;
        }
        width = w;
        height = h;
    }
    camera_update(&camera);
    draw_world(commands);
    // draw_ui(commands);
    SDL_SubmitGPUCommandBuffer(commands);
}

////////////////////////////////////////////////////////////////////////////////
// Events //////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static bool poll()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            return false;
        case SDL_EVENT_WINDOW_RESIZED:
            camera_size(&camera, event.window.data1, event.window.data2);
            break;
        case SDL_EVENT_MOUSE_MOTION:
            if (SDL_GetWindowRelativeMouseMode(window)) {
                float sensitivity = 0.1f;
                float yaw = event.motion.xrel * sensitivity;
                float pitch = -event.motion.yrel * sensitivity;
                camera_rotate(&camera, pitch, yaw);
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            SDL_SetWindowRelativeMouseMode(window, 1);
            if (event.button.button == SDL_BUTTON_LEFT) {
                float s, t, p;
                camera_vector(&camera, &s, &t, &p);
                float length = sqrtf(s * s + t * t + p * p);
                s /= length;
                t /= length;
                p /= length;
                float a, b, c;
                for (float i = 0; i < 10.0f; i += 0.0001f) {
                    float x, y, z;
                    camera_position(&camera, &x, &y, &z);
                    a = x + s * i;
                    b = y + t * i;
                    c = z + p * i;
                    block_t block = world_get_block(a, b, c);
                    if (block) {
                        world_set_block(a, b, c, BLOCK_EMPTY);
                        break;
                    }
                }
            }
            break;
        case SDL_EVENT_KEY_DOWN:
            if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                SDL_SetWindowRelativeMouseMode(window, 0);
            }
            break;
        }
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Movement ////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static void move()
{
    float dx = 0.0f;
    float dy = 0.0f;
    float dz = 0.0f;
    if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_D]) dx++;
    if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_A]) dx--;
    if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_E]) dy++;
    if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_Q]) dy--;
    if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_W]) dz++;
    if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_S]) dz--;
    float speed = 1.2f;
    dx *= speed;
    dy *= speed;
    dz *= speed;
    camera_move(&camera, dx, dy, dz);
}

////////////////////////////////////////////////////////////////////////////////
// Main ////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv)
{
    (void) argc;
    (void) argv;

    SDL_SetAppMetadata("blocks", NULL, NULL);
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return EXIT_FAILURE;
    }
    window = SDL_CreateWindow("blocks", 1024, 764, 0);
    if (!window) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        return EXIT_FAILURE;
    }
    device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, 1, NULL);
    if (!device) {
        SDL_Log("Failed to create device: %s", SDL_GetError());
        return false;
    }
    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        SDL_Log("Failed to create swapchain: %s", SDL_GetError());
        return false;
    }
    load_atlas();
    load_world_pipeline();
    load_ui_pipeline();
    create_ui_vbo();
    noise_init(NOISE_CUBE);
    if (!world_init(device)) {
        return EXIT_FAILURE;
    }
    SDL_SetWindowResizable(window, true);
    SDL_Surface* icon = get_atlas_icon(BLOCK_GRASS);
    SDL_SetWindowIcon(window, icon);
    SDL_DestroySurface(icon);
    camera_init(&camera);
    camera_move(&camera, 0, 30, 0);
    int loop = 1;
    while (loop) {
        if (!poll()) {
            break;
        }
        move();
        float x, y, z;
        camera_position(&camera, &x, &y, &z);
        world_update(x, y, z);
        draw();
    }
    world_free();
    SDL_DestroySurface(atlas_surface);
    SDL_ReleaseGPUTexture(device, atlas_texture);
    SDL_ReleaseGPUSampler(device, atlas_sampler);
    SDL_ReleaseGPUGraphicsPipeline(device, world_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(device, ui_pipeline);
    SDL_ReleaseGPUBuffer(device, ui_vbo);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_SUCCESS;
}