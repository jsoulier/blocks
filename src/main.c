#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "camera.h"
#include "noise.h"
#include "save.h"
#include "shader.h"
#include "world.h"

static SDL_Window* window;
static SDL_GPUDevice* device;
static SDL_GPUTextureFormat colorTextureFormat;
static SDL_GPUTextureFormat depthTextureFormat;
static SDL_GPUTexture* depthTexture;
static SDL_GPUGraphicsPipeline* chunkPipeline;
static World world;
static Save save;
static Camera camera;
static Noise noise;

static bool LoadChunkPipeline()
{
    SDL_GPUGraphicsPipelineCreateInfo info =
    {
        .vertex_shader = LoadShader(device, "chunk.vert"),
        .fragment_shader = LoadShader(device, "chunk.frag"),
        .target_info =
        {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[])
            {{
                .format = colorTextureFormat,
            }},
            .has_depth_stencil_target = true,
            .depth_stencil_format = depthTextureFormat,
        },
        .vertex_input_state =
        {
            .num_vertex_attributes = 1,
            .vertex_attributes = (SDL_GPUVertexAttribute[])
            {{
                .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT,
            }},
            .num_vertex_buffers = 1,
            .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[])
            {{
                .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                .pitch = 4,
            }},
        },
        .depth_stencil_state =
        {
            .enable_depth_test = true,
            .enable_depth_write = true,
            .compare_op = SDL_GPU_COMPAREOP_LESS,
        },
        .rasterizer_state =
        {
            .cull_mode = SDL_GPU_CULLMODE_BACK,
            .front_face = SDL_GPU_FRONTFACE_CLOCKWISE,
        },
    };
    if (info.vertex_shader && info.fragment_shader)
    {
        chunkPipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return chunkPipeline != NULL;
}

SDL_AppResult SDLCALL SDL_AppInit(void** appstate, int argc, char** argv)
{
#ifndef NDEBUG
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
#endif
    SDL_SetAppMetadata("Voxel Raytracer", NULL, NULL);
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    window = SDL_CreateWindow("Blocks", 960, 720, SDL_WINDOW_HIDDEN);
    if (!window)
    {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
#ifndef NDEBUG
    device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL, true, NULL);
#else
    device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL, false, NULL);
#endif
    if (!device)
    {
        SDL_Log("Failed to create device: %s", SDL_GetError());
        return false;
    }
    if (!SDL_ClaimWindowForGPUDevice(device, window))
    {
        SDL_Log("Failed to claim window: %s", SDL_GetError());
        return false;
    }
    if (SDL_WindowSupportsGPUPresentMode(device, window, SDL_GPU_PRESENTMODE_MAILBOX))
    {
        SDL_SetGPUSwapchainParameters(device, window,
            SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_MAILBOX);
    }
    colorTextureFormat = SDL_GetGPUSwapchainTextureFormat(device, window);
    if (SDL_GPUTextureSupportsFormat(device, SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET))
    {
        depthTextureFormat = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    }
    else
    {
        depthTextureFormat = SDL_GPU_TEXTUREFORMAT_D24_UNORM;
    }
    if (!LoadChunkPipeline())
    {
        SDL_Log("Failed to create chunk pipeline: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_ShowWindow(window);
    SDL_SetWindowResizable(window, true);
    SDL_FlashWindow(window, SDL_FLASH_BRIEFLY);
    CreateNoise(&noise, NoiseTypeDefault, 1337);
    CreateWorld(&world, device);
    CreateCamera(&camera, CameraTypePerspective);
    return SDL_APP_CONTINUE;
}

void SDLCALL SDL_AppQuit(void* appstate, SDL_AppResult result)
{
    DestroyWorld(&world);
    SDL_ReleaseGPUGraphicsPipeline(device, chunkPipeline);
    SDL_ReleaseGPUTexture(device, depthTexture);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

SDL_AppResult SDLCALL SDL_AppIterate(void* appstate)
{
    UpdateWorld(&world, &camera, &save, &noise);
    SDL_GPUCommandBuffer* commandBuffer = SDL_AcquireGPUCommandBuffer(device);
    if (!commandBuffer)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return SDL_APP_CONTINUE;
    }
    SDL_GPUTexture* swapchainTexture;
    Uint32 width;
    Uint32 height;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(commandBuffer, window, &swapchainTexture, &width, &height))
    {
        SDL_Log("Failed to acquire swapchain texture: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(commandBuffer);
        return SDL_APP_CONTINUE;
    }
    if (!swapchainTexture || !width || !height)
    {
        SDL_SubmitGPUCommandBuffer(commandBuffer);
        return SDL_APP_CONTINUE;
    }
    if (width != camera.Width || height != camera.Height)
    {
        SDL_ReleaseGPUTexture(device, depthTexture);
        SDL_GPUTextureCreateInfo info = {0};
        info.type = SDL_GPU_TEXTURETYPE_2D;
        info.format = depthTextureFormat;
        info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        info.width = width;
        info.height = height;
        info.layer_count_or_depth = 1;
        info.num_levels = 1;
        depthTexture = SDL_CreateGPUTexture(device, &info);
        if (!depthTexture)
        {
            SDL_Log("Failed to create depth texture: %s", SDL_GetError());
            SDL_SubmitGPUCommandBuffer(commandBuffer);
            return SDL_APP_CONTINUE;
        }
        SetCameraViewport(&camera, width, height);
    }
    {
        SDL_PushGPUDebugGroup(commandBuffer, "chunk");
        DrawWorld(&world, &camera);
        SDL_PopGPUDebugGroup(commandBuffer);
    }
    SDL_SubmitGPUCommandBuffer(commandBuffer);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDLCALL SDL_AppEvent(void* appstate, SDL_Event* event)
{
    switch (event->type)
    {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_MOUSE_WHEEL:
    case SDL_EVENT_MOUSE_MOTION:
        break;
    }
    return SDL_APP_CONTINUE;
}
