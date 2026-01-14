#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "camera.h"
#include "check.h"
#include "save.h"
#include "shader.h"
#include "world.h"

static const float ATLAS_WIDTH = 512.0f;
static const int ATLAS_MIP_LEVELS = 4;
static const float BLOCK_WIDTH = 16.0f;
static const char* SAVE_PATH = "blocks.sqlite3";
static const int SHADOW_RESOLUTION = 4096.0f;
static const float SPEED = 0.01f;
static const float SENSITIVITY = 0.1f;
static const float RANGE = 10.0f;
static const float SHADOW_ORTHO = 300.0f;
static const float SHADOW_FAR = 300.0f;
static const float SHADOW_PITCH = -45.0f;
static const float SHADOW_YAW = 45.0f;
static const float SHADOW_Y = 30.0f;

typedef struct player
{
    int id;
    camera_t camera;
    world_raycast_t query;
    block_t block;
}
player_t;

typedef struct sky
{
    camera_t camera;
}
sky_t;

static SDL_Window* window;
static SDL_GPUDevice* device;
static SDL_GPUTextureFormat color_format;
static SDL_GPUTextureFormat depth_format;
static SDL_GPUGraphicsPipeline* opaque_pipeline;
static SDL_GPUGraphicsPipeline* transparent_pipeline;
static SDL_GPUGraphicsPipeline* predepth_pipeline;
static SDL_GPUGraphicsPipeline* shadow_pipeline;
static SDL_GPUGraphicsPipeline* sky_pipeline;
static SDL_GPUGraphicsPipeline* raycast_pipeline;
static SDL_GPUComputePipeline* ui_pipeline;
static SDL_GPUComputePipeline* ssao_pipeline;
static SDL_GPUComputePipeline* composite_pipeline;
static SDL_GPUComputePipeline* fxaa_pipeline;
static SDL_GPUComputePipeline* blur_pipeline;
static SDL_Surface* atlas_surface;
static SDL_GPUTexture* atlas_texture;
static SDL_GPUTexture* depth_texture;
static SDL_GPUTexture* color_texture;
static SDL_GPUTexture* position_texture;
static SDL_GPUTexture* light_texture;
static SDL_GPUTexture* voxel_texture;
static SDL_GPUTexture* ssao_texture;
static SDL_GPUTexture* ssao_blur_texture;
static SDL_GPUTexture* composite_texture;
static SDL_GPUTexture* fxaa_texture;
static SDL_GPUTexture* shadow_texture;
static SDL_GPUSampler* linear_sampler;
static SDL_GPUSampler* nearest_sampler;
static SDL_GPUSampler* nearest_anisotropy_sampler;
static player_t player;
static sky_t sky;
static Uint64 ticks1;

void sky_init(sky_t* sky)
{
    camera_init(&sky->camera, CAMERA_TYPE_ORTHO);
    sky->camera.ortho = SHADOW_ORTHO;
    sky->camera.far = SHADOW_FAR;
}

void sky_update(sky_t* sky, camera_t* camera, float dt)
{
    float x;
    float y;
    float z;
    camera_get_position(camera, &x, &y, &z);
    x = SDL_floor(x / CHUNK_WIDTH) * CHUNK_WIDTH;
    y = SHADOW_Y;
    z = SDL_floor(z / CHUNK_WIDTH) * CHUNK_WIDTH;
    camera_set_position(&sky->camera, x, y, z);
    camera_set_rotation(&sky->camera, SHADOW_PITCH, SHADOW_YAW, 0.0f);
    camera_update(&sky->camera);
}

static void query(player_t* player)
{
    float x;
    float y;
    float z;
    float dx;
    float dy;
    float dz;
    camera_get_position(&player->camera, &x, &y, &z);
    camera_get_vector(&player->camera, &dx, &dy, &dz);
    player->query = world_raycast(x, y, z, dx, dy, dz, RANGE);
}

void player_init(player_t* player)
{
    player->id = 0;
    camera_init(&player->camera, CAMERA_TYPE_PERSPECTIVE);
    player->block = BLOCK_YELLOW_TORCH;
}

void player_move(player_t* player, float dt)
{
    float speed = SPEED;
    float dx = 0.0f;
    float dy = 0.0f;
    float dz = 0.0f;
    const bool* state = SDL_GetKeyboardState(NULL);
    dx += state[SDL_SCANCODE_D];
    dx -= state[SDL_SCANCODE_A];
    dy += state[SDL_SCANCODE_E] || state[SDL_SCANCODE_SPACE];
    dy -= state[SDL_SCANCODE_Q] || state[SDL_SCANCODE_LSHIFT];
    dz += state[SDL_SCANCODE_W];
    dz -= state[SDL_SCANCODE_S];
    if (state[SDL_SCANCODE_LCTRL])
    {
        speed *= 5.0f;
    }
    dx *= speed * dt;
    dy *= speed * dt;
    dz *= speed * dt;
    camera_move(&player->camera, dx, dy, dz);
    query(player);
}

void player_rotate(player_t* player, float pitch, float yaw)
{
    pitch *= -SENSITIVITY;
    yaw *= SENSITIVITY;
    camera_rotate(&player->camera, pitch, yaw);
    query(player);
}

void player_break(player_t* player)
{
    if (player->query.block != BLOCK_EMPTY)
    {
        world_set_block(player->query.current, BLOCK_EMPTY);
    }
}

void player_place(player_t* player)
{
    if (player->query.block != BLOCK_EMPTY)
    {
        world_set_block(player->query.previous, player->block);
    }
}

void player_scroll(player_t* player, int dy)
{
    static const int COUNT = BLOCK_COUNT - BLOCK_EMPTY - 1;
    int block = player->block - (BLOCK_EMPTY + 1) + dy;
    block = (block + COUNT) % COUNT;
    player->block = block + BLOCK_EMPTY + 1;
}

typedef struct player_save_data
{
    float x;
    float y;
    float z;
    float pitch;
    float yaw;
    float roll;
    block_t block;
}
player_save_data_t;

void player_load(player_t* player)
{
    player_save_data_t data;
    if (save_get_player(player->id, &data, sizeof(data)))
    {
        player->block = data.block;
        camera_set_position(&player->camera, data.x, data.y, data.z);
        camera_set_rotation(&player->camera, data.pitch, data.yaw, data.roll);
    }
}

void player_save(player_t* player)
{
    player_save_data_t data;
    camera_get_position(&player->camera, &data.x, &data.y, &data.z);
    camera_get_rotation(&player->camera, &data.pitch, &data.yaw, &data.roll);
    data.block = player->block;
    save_set_player(player->id, &data, sizeof(data));
}
static bool create_atlas()
{
    char path[512] = {0};
    SDL_snprintf(path, sizeof(path), "%satlas.png", SDL_GetBasePath());
    atlas_surface = SDL_LoadPNG(path);
    if (!atlas_surface)
    {
        SDL_Log("Failed to create atlas surface: %s", SDL_GetError());
        return false;
    }
    SDL_GPUTexture* texture = NULL;
    SDL_GPUTransferBuffer* buffer = NULL;
    {
        SDL_GPUTextureCreateInfo info = {0};
        info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        info.type = SDL_GPU_TEXTURETYPE_2D_ARRAY;
        info.layer_count_or_depth = ATLAS_WIDTH / BLOCK_WIDTH;
        info.num_levels = ATLAS_MIP_LEVELS;
        info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        info.width = BLOCK_WIDTH;
        info.height = BLOCK_WIDTH;
        atlas_texture = SDL_CreateGPUTexture(device, &info);
        if (!atlas_texture)
        {
            SDL_Log("Failed to create atlas texture: %s", SDL_GetError());
            return false;
        }
        info.type = SDL_GPU_TEXTURETYPE_2D;
        info.layer_count_or_depth = 1;
        info.num_levels = 1;
        info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        info.width = atlas_surface->w;
        info.height = atlas_surface->h;
        texture = SDL_CreateGPUTexture(device, &info);
        if (!texture)
        {
            SDL_Log("Failed to create texture: %s", SDL_GetError());
            return false;
        }
    }
    {
        SDL_GPUTransferBufferCreateInfo info = {0};
        info.size = atlas_surface->w * atlas_surface->h * 4;
        info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        buffer = SDL_CreateGPUTransferBuffer(device, &info);
        if (!buffer)
        {
            SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
            return false;
        }
    }
    void* data = SDL_MapGPUTransferBuffer(device, buffer, 0);
    if (!data)
    {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        return false;
    }
    SDL_memcpy(data, atlas_surface->pixels, atlas_surface->w * atlas_surface->h * 4);
    SDL_UnmapGPUTransferBuffer(device, buffer);
    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(device);
    if (!command_buffer)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return false;
    }
    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    if (!copy_pass)
    {
        SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
        return false;
    }
    SDL_GPUTextureTransferInfo info = {0};
    SDL_GPUTextureRegion region = {0};
    info.transfer_buffer = buffer;
    region.texture = texture;
    region.w = atlas_surface->w;
    region.h = atlas_surface->h;
    region.d = 1;
    SDL_UploadToGPUTexture(copy_pass, &info, &region, 0);
    SDL_EndGPUCopyPass(copy_pass);
    for (int i = 0; i < ATLAS_WIDTH / BLOCK_WIDTH; i++)
    {
        SDL_GPUBlitInfo info = {0};
        info.source.texture = texture;
        info.source.x = i * BLOCK_WIDTH;
        info.source.y = 0;
        info.source.w = BLOCK_WIDTH;
        info.source.h = BLOCK_WIDTH;
        info.destination.texture = atlas_texture;
        info.destination.x = 0;
        info.destination.y = 0;
        info.destination.w = BLOCK_WIDTH;
        info.destination.h = BLOCK_WIDTH;
        info.destination.layer_or_depth_plane = i;
        SDL_BlitGPUTexture(command_buffer, &info);
    }
    SDL_GenerateMipmapsForGPUTexture(command_buffer, atlas_texture);
    SDL_SubmitGPUCommandBuffer(command_buffer);
    SDL_ReleaseGPUTexture(device, texture);
    SDL_ReleaseGPUTransferBuffer(device, buffer);
    return true;
}

SDL_Surface* create_icon(block_t block)
{
    if (!atlas_surface)
    {
        return NULL;
    }
    SDL_Surface* icon = SDL_CreateSurface(BLOCK_WIDTH, BLOCK_WIDTH, SDL_PIXELFORMAT_RGBA32);
    if (!icon)
    {
        SDL_Log("Failed to create icon surface: %s", SDL_GetError());
        return NULL;
    }
    SDL_Rect src;
    src.x = block_get_index(block, DIRECTION_NORTH) * BLOCK_WIDTH;
    src.y = 0;
    src.w = BLOCK_WIDTH;
    src.h = BLOCK_WIDTH;
    SDL_Rect dst;
    dst.x = 0;
    dst.y = 0;
    dst.w = BLOCK_WIDTH;
    dst.h = BLOCK_WIDTH;
    if (!SDL_BlitSurface(atlas_surface, &src, icon, &dst))
    {
        SDL_Log("Failed to blit icon surface: %s", SDL_GetError());
        SDL_DestroySurface(icon);
        return NULL;
    }
    return icon;
}

static void set_icon()
{
    SDL_Surface* icon = create_icon(BLOCK_GRASS);
    SDL_SetWindowIcon(window, icon);
    SDL_DestroySurface(icon);
}

static bool create_samplers()
{
    SDL_GPUSamplerCreateInfo info = {0};
    info.min_filter = SDL_GPU_FILTER_LINEAR;
    info.mag_filter = SDL_GPU_FILTER_LINEAR;
    info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    linear_sampler = SDL_CreateGPUSampler(device, &info);
    if (!linear_sampler)
    {
        SDL_Log("Failed to create linear sampler: %s", SDL_GetError());
        return false;
    }
    info.min_filter = SDL_GPU_FILTER_NEAREST;
    info.mag_filter = SDL_GPU_FILTER_NEAREST;
    nearest_sampler = SDL_CreateGPUSampler(device, &info);
    if (!nearest_sampler)
    {
        SDL_Log("Failed to create nearest sampler: %s", SDL_GetError());
        return false;
    }
    info.enable_anisotropy = true;
    info.max_anisotropy = 16.0f;
    nearest_anisotropy_sampler = SDL_CreateGPUSampler(device, &info);
    if (!nearest_anisotropy_sampler)
    {
        SDL_Log("Failed to create nearest anisotropy sampler: %s", SDL_GetError());
        return false;
    }
    return true;
}

static bool create_opaque_pipeline()
{
    SDL_GPUGraphicsPipelineCreateInfo info =
    {
        .vertex_shader = shader_load(device, "opaque.vert"),
        .fragment_shader = shader_load(device, "opaque.frag"),
        .target_info =
        {
            .num_color_targets = 4,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[])
            {{
                .format = color_format,
            },
            {
                .format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
            },
            {
                .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
            },
            {
                .format = SDL_GPU_TEXTUREFORMAT_R8_UINT,
            }},
            .has_depth_stencil_target = true,
            .depth_stencil_format = depth_format,
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
        opaque_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return opaque_pipeline != NULL;
}

static bool create_predepth_pipeline()
{
    SDL_GPUGraphicsPipelineCreateInfo info =
    {
        .vertex_shader = shader_load(device, "predepth.vert"),
        .fragment_shader = shader_load(device, "empty.frag"),
        .target_info =
        {
            .has_depth_stencil_target = true,
            .depth_stencil_format = depth_format,
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
    };
    if (info.vertex_shader && info.fragment_shader)
    {
        predepth_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return predepth_pipeline != NULL;
}

static bool create_transparent_pipeline()
{
    SDL_GPUGraphicsPipelineCreateInfo info =
    {
        .vertex_shader = shader_load(device, "chunk.vert"),
        .fragment_shader = shader_load(device, "transparent.frag"),
        .target_info =
        {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[])
            {{
                .format = color_format,
                .blend_state =
                {
                    .enable_blend = true,
                    .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                    .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                    .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                    .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                    .color_blend_op = SDL_GPU_BLENDOP_ADD,
                    .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
                },
            }},
            .has_depth_stencil_target = true,
            .depth_stencil_format = depth_format,
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
            .compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
        },
        .rasterizer_state =
        {
            .cull_mode = SDL_GPU_CULLMODE_BACK,
            .front_face = SDL_GPU_FRONTFACE_CLOCKWISE,
        },
    };
    if (info.vertex_shader && info.fragment_shader)
    {
        transparent_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return transparent_pipeline != NULL;
}

static bool create_shadow_pipeline()
{
    SDL_GPUGraphicsPipelineCreateInfo info =
    {
        .vertex_shader = shader_load(device, "shadow.vert"),
        .fragment_shader = shader_load(device, "empty.frag"),
        .target_info =
        {
            .has_depth_stencil_target = true,
            .depth_stencil_format = depth_format,
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
    };
    if (info.vertex_shader && info.fragment_shader)
    {
        shadow_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return shadow_pipeline != NULL;
}

static bool create_sky_pipeline()
{
    SDL_GPUGraphicsPipelineCreateInfo info =
    {
        .vertex_shader = shader_load(device, "sky.vert"),
        .fragment_shader = shader_load(device, "sky.frag"),
        .target_info =
        {
            .num_color_targets = 4,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[])
            {{
                .format = color_format,
            },
            {
                .format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
            },
            {
                .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
            },
            {
                .format = SDL_GPU_TEXTUREFORMAT_R8_UINT,
            }},
            .has_depth_stencil_target = true,
            .depth_stencil_format = depth_format,
        },
    };
    if (info.vertex_shader && info.fragment_shader)
    {
        sky_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return sky_pipeline != NULL;
}

static bool create_raycast_pipeline()
{
    SDL_GPUGraphicsPipelineCreateInfo info =
    {
        .vertex_shader = shader_load(device, "raycast.vert"),
        .fragment_shader = shader_load(device, "raycast.frag"),
        .target_info =
        {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[])
            {{
                .format = color_format,
                .blend_state =
                {
                    .enable_blend = true,
                    .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                    .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                    .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                    .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                    .color_blend_op = SDL_GPU_BLENDOP_ADD,
                    .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
                },
            }},
            .has_depth_stencil_target = true,
            .depth_stencil_format = depth_format,
        },
        .depth_stencil_state =
        {
            .enable_depth_test = true,
            .compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
        },
    };
    if (info.vertex_shader && info.fragment_shader)
    {
        raycast_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return raycast_pipeline != NULL;
}

SDL_AppResult SDLCALL SDL_AppInit(void** appstate, int argc, char** argv)
{
#ifndef NDEBUG
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
#endif
    SDL_SetAppMetadata("Blocks", NULL, NULL);
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
    SDL_SetGPUSwapchainParameters(device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_MAILBOX);
    color_format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    depth_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    if (!create_atlas())
    {
        SDL_Log("Failed to create atlas: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!create_samplers())
    {
        SDL_Log("Failed to create samplers: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!create_opaque_pipeline())
    {
        SDL_Log("Failed to create opaque pipeline: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!create_transparent_pipeline())
    {
        SDL_Log("Failed to create transparent pipeline: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!create_predepth_pipeline())
    {
        SDL_Log("Failed to create predepth pipeline: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!create_shadow_pipeline())
    {
        SDL_Log("Failed to create shadow pipeline: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!create_sky_pipeline())
    {
        SDL_Log("Failed to create sky pipeline: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!create_raycast_pipeline())
    {
        SDL_Log("Failed to create raycast pipeline: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    ui_pipeline = shader_load(device, "ui.comp");
    if (!ui_pipeline)
    {
        SDL_Log("Failed to load ui pipeline");
        return SDL_APP_FAILURE;
    }
    ssao_pipeline = shader_load(device, "ssao.comp");
    if (!ssao_pipeline)
    {
        SDL_Log("Failed to load ssao pipeline");
        return SDL_APP_FAILURE;
    }
    composite_pipeline = shader_load(device, "composite.comp");
    if (!composite_pipeline)
    {
        SDL_Log("Failed to load composite pipeline");
        return SDL_APP_FAILURE;
    }
    fxaa_pipeline = shader_load(device, "fxaa.comp");
    if (!fxaa_pipeline)
    {
        SDL_Log("Failed to load fxaa pipeline");
        return SDL_APP_FAILURE;
    }
    blur_pipeline = shader_load(device, "blur.comp");
    if (!blur_pipeline)
    {
        SDL_Log("Failed to load blur pipeline");
        return SDL_APP_FAILURE;
    }
    {
        SDL_GPUTextureCreateInfo info = {0};
        info.type = SDL_GPU_TEXTURETYPE_2D;
        info.format = depth_format;
        info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
        info.width = SHADOW_RESOLUTION;
        info.height = SHADOW_RESOLUTION;
        info.layer_count_or_depth = 1;
        info.num_levels = 1;
        shadow_texture = SDL_CreateGPUTexture(device, &info);
        if (!shadow_texture)
        {
            SDL_Log("Failed to create shadow texture: %s", SDL_GetError());
            return false;
        }
    }
    SDL_ShowWindow(window);
    SDL_SetWindowResizable(window, true);
    SDL_FlashWindow(window, SDL_FLASH_BRIEFLY);
    set_icon();
    save_init(SAVE_PATH);
    world_init(device);
    player_init(&player);
    sky_init(&sky);
    player_load(&player);
    ticks1 = SDL_GetTicks();
    return SDL_APP_CONTINUE;
}

void SDLCALL SDL_AppQuit(void* appstate, SDL_AppResult result)
{
    SDL_HideWindow(window);
    world_free();
    save_free();
    SDL_ReleaseGPUSampler(device, nearest_anisotropy_sampler);
    SDL_ReleaseGPUSampler(device, linear_sampler);
    SDL_ReleaseGPUSampler(device, nearest_sampler);
    SDL_ReleaseGPUTexture(device, shadow_texture);
    SDL_ReleaseGPUTexture(device, fxaa_texture);
    SDL_ReleaseGPUTexture(device, composite_texture);
    SDL_ReleaseGPUTexture(device, ssao_blur_texture);
    SDL_ReleaseGPUTexture(device, ssao_texture);
    SDL_ReleaseGPUTexture(device, color_texture);
    SDL_ReleaseGPUTexture(device, light_texture);
    SDL_ReleaseGPUTexture(device, position_texture);
    SDL_ReleaseGPUTexture(device, voxel_texture);
    SDL_ReleaseGPUTexture(device, depth_texture);
    SDL_ReleaseGPUTexture(device, atlas_texture);
    SDL_DestroySurface(atlas_surface);
    SDL_ReleaseGPUComputePipeline(device, blur_pipeline);
    SDL_ReleaseGPUComputePipeline(device, fxaa_pipeline);
    SDL_ReleaseGPUComputePipeline(device, composite_pipeline);
    SDL_ReleaseGPUComputePipeline(device, ssao_pipeline);
    SDL_ReleaseGPUComputePipeline(device, ui_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(device, raycast_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(device, sky_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(device, shadow_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(device, predepth_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(device, transparent_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(device, opaque_pipeline);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

static bool resize(int width, int height)
{
    SDL_ReleaseGPUTexture(device, depth_texture);
    SDL_ReleaseGPUTexture(device, color_texture);
    SDL_ReleaseGPUTexture(device, voxel_texture);
    SDL_ReleaseGPUTexture(device, position_texture);
    SDL_ReleaseGPUTexture(device, ssao_texture);
    SDL_ReleaseGPUTexture(device, ssao_blur_texture);
    SDL_ReleaseGPUTexture(device, composite_texture);
    SDL_ReleaseGPUTexture(device, fxaa_texture);
    SDL_ReleaseGPUTexture(device, light_texture);
    depth_texture = NULL;
    color_texture = NULL;
    voxel_texture = NULL;
    position_texture = NULL;
    ssao_texture = NULL;
    ssao_blur_texture = NULL;
    composite_texture = NULL;
    fxaa_texture = NULL;
    light_texture = NULL;
    SDL_GPUTextureCreateInfo info = {0};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = depth_format;
    info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    depth_texture = SDL_CreateGPUTexture(device, &info);
    if (!depth_texture)
    {
        SDL_Log("Failed to create depth texture: %s", SDL_GetError());
        return false;
    }
    info.format = color_format;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;
    color_texture = SDL_CreateGPUTexture(device, &info);
    if (!color_texture)
    {
        SDL_Log("Failed to create color texture: %s", SDL_GetError());
        return false;
    }
    info.format = SDL_GPU_TEXTUREFORMAT_R8_UINT;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;
    voxel_texture = SDL_CreateGPUTexture(device, &info);
    if (!voxel_texture)
    {
        SDL_Log("Failed to create voxel texture: %s", SDL_GetError());
        return false;
    }
    info.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;
    position_texture = SDL_CreateGPUTexture(device, &info);
    if (!position_texture)
    {
        SDL_Log("Failed to create position texture: %s", SDL_GetError());
        return false;
    }
    info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;
    light_texture = SDL_CreateGPUTexture(device, &info);
    if (!light_texture)
    {
        SDL_Log("Failed to create light texture: %s", SDL_GetError());
        return false;
    }
    info.format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;
    ssao_texture = SDL_CreateGPUTexture(device, &info);
    if (!ssao_texture)
    {
        SDL_Log("Failed to create ssao texture: %s", SDL_GetError());
        return false;
    }
    info.format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;
    ssao_blur_texture = SDL_CreateGPUTexture(device, &info);
    if (!ssao_blur_texture)
    {
        SDL_Log("Failed to create ssao blur texture: %s", SDL_GetError());
        return false;
    }
    info.format = color_format;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    composite_texture = SDL_CreateGPUTexture(device, &info);
    if (!composite_texture)
    {
        SDL_Log("Failed to create composite texture: %s", SDL_GetError());
        return false;
    }
    info.format = color_format;
    info.usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    fxaa_texture = SDL_CreateGPUTexture(device, &info);
    if (!fxaa_texture)
    {
        SDL_Log("Failed to create fxaa texture: %s", SDL_GetError());
        return false;
    }
    camera_set_viewport(&player.camera, width, height);
    return true;
}

static void render_sky(SDL_GPUCommandBuffer* command_buffer)
{
    SDL_GPUDepthStencilTargetInfo depth_info = {0};
    depth_info.load_op = SDL_GPU_LOADOP_CLEAR;
    depth_info.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
    depth_info.store_op = SDL_GPU_STOREOP_STORE;
    depth_info.texture = shadow_texture;
    depth_info.clear_depth = 1.0f;
    depth_info.cycle = true;
    SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command_buffer, NULL, 0, &depth_info);
    if (!render_pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    {
        world_pass_t data;
        data.camera = &sky.camera;
        data.command_buffer = command_buffer;
        data.render_pass = render_pass;
        data.lights = false;
        data.mesh = WORLD_MESH_OPAQUE;
        SDL_BindGPUGraphicsPipeline(render_pass, shadow_pipeline);
        world_render(&data);
    }
    SDL_EndGPURenderPass(render_pass);
}

static void render_geometry(SDL_GPUCommandBuffer* command_buffer)
{
    SDL_GPUColorTargetInfo color_info[4] = {0};
    color_info[0].load_op = SDL_GPU_LOADOP_CLEAR;
    color_info[0].texture = color_texture;
    color_info[0].cycle = true;
    color_info[0].store_op = SDL_GPU_STOREOP_STORE;
    color_info[1].load_op = SDL_GPU_LOADOP_CLEAR;
    color_info[1].texture = position_texture;
    color_info[1].cycle = true;
    color_info[1].store_op = SDL_GPU_STOREOP_STORE;
    color_info[2].load_op = SDL_GPU_LOADOP_CLEAR;
    color_info[2].texture = light_texture;
    color_info[2].cycle = true;
    color_info[2].store_op = SDL_GPU_STOREOP_STORE;
    color_info[3].load_op = SDL_GPU_LOADOP_CLEAR;
    color_info[3].texture = voxel_texture;
    color_info[3].cycle = true;
    color_info[3].store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPUDepthStencilTargetInfo depth_info = {0};
    depth_info.load_op = SDL_GPU_LOADOP_CLEAR;
    depth_info.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
    depth_info.store_op = SDL_GPU_STOREOP_STORE;
    depth_info.texture = depth_texture;
    depth_info.clear_depth = 1.0f;
    depth_info.cycle = true;
    SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command_buffer, color_info, 4, &depth_info);
    if (!render_pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    {
        SDL_PushGPUDebugGroup(command_buffer, "sky");
        SDL_BindGPUGraphicsPipeline(render_pass, sky_pipeline);
        SDL_PushGPUVertexUniformData(command_buffer, 0, player.camera.proj, 64);
        SDL_PushGPUVertexUniformData(command_buffer, 1, player.camera.view, 64);
        SDL_DrawGPUPrimitives(render_pass, 36, 1, 0, 0);
        SDL_PopGPUDebugGroup(command_buffer);
    }
    {
        world_pass_t data;
        data.mesh = WORLD_MESH_OPAQUE;
        data.camera = &player.camera;
        data.command_buffer = command_buffer;
        data.render_pass = render_pass;
        data.lights = true;
        SDL_GPUTextureSamplerBinding atlas_binding = {atlas_texture, nearest_anisotropy_sampler};
        SDL_BindGPUGraphicsPipeline(render_pass, opaque_pipeline);
        SDL_BindGPUFragmentSamplers(render_pass, 0, &atlas_binding, 1);
        world_render(&data);
    }
    SDL_EndGPURenderPass(render_pass);
}

static void postprocess(SDL_GPUCommandBuffer* command_buffer)
{
    {
        SDL_GPUStorageTextureReadWriteBinding write_textures = {ssao_texture};
        SDL_GPUComputePass* compute_pass = SDL_BeginGPUComputePass(command_buffer, &write_textures, 1, NULL, 0);
        if (!compute_pass)
        {
            SDL_Log("Failed to begin compute pass: %s", SDL_GetError());
            return;
        }
        SDL_GPUTexture* read_textures[2] = {voxel_texture, position_texture};
        int groups_x = (player.camera.width + 8 - 1) / 8;
        int groups_y = (player.camera.height + 8 - 1) / 8;
        SDL_PushGPUDebugGroup(command_buffer, "ssao");
        SDL_BindGPUComputePipeline(compute_pass, ssao_pipeline);
        SDL_BindGPUComputeStorageTextures(compute_pass, 0, read_textures, 2);
        SDL_DispatchGPUCompute(compute_pass, groups_x, groups_y, 1);
        SDL_EndGPUComputePass(compute_pass);
        SDL_PopGPUDebugGroup(command_buffer);
    }
    {
        SDL_GPUStorageTextureReadWriteBinding write_textures = {ssao_blur_texture};
        SDL_GPUComputePass* compute_pass = SDL_BeginGPUComputePass(command_buffer, &write_textures, 1, NULL, 0);
        if (!compute_pass)
        {
            SDL_Log("Failed to begin compute pass: %s", SDL_GetError());
            return;
        }
        SDL_GPUTexture* read_textures[1] = {ssao_texture};
        int groups_x = (player.camera.width + 8 - 1) / 8;
        int groups_y = (player.camera.height + 8 - 1) / 8;
        SDL_PushGPUDebugGroup(command_buffer, "blur");
        SDL_BindGPUComputePipeline(compute_pass, blur_pipeline);
        SDL_BindGPUComputeStorageTextures(compute_pass, 0, read_textures, 1);
        SDL_DispatchGPUCompute(compute_pass, groups_x, groups_y, 1);
        SDL_EndGPUComputePass(compute_pass);
        SDL_PopGPUDebugGroup(command_buffer);
    }
    {
        SDL_GPUStorageTextureReadWriteBinding write_textures = {composite_texture};
        SDL_GPUComputePass* compute_pass = SDL_BeginGPUComputePass(command_buffer, &write_textures, 1, NULL, 0);
        if (!compute_pass)
        {
            SDL_Log("Failed to begin compute pass: %s", SDL_GetError());
            return;
        }
        SDL_GPUTexture* read_textures[5] = {color_texture, light_texture, ssao_blur_texture, voxel_texture, position_texture};
        SDL_GPUTextureSamplerBinding read_samplers = {shadow_texture, nearest_sampler};
        int groups_x = (player.camera.width + 8 - 1) / 8;
        int groups_y = (player.camera.height + 8 - 1) / 8;
        SDL_PushGPUDebugGroup(command_buffer, "composite");
        SDL_BindGPUComputePipeline(compute_pass, composite_pipeline);
        SDL_BindGPUComputeStorageTextures(compute_pass, 0, read_textures, 5);
        SDL_BindGPUComputeSamplers(compute_pass, 0, &read_samplers, 1);
        SDL_PushGPUComputeUniformData(command_buffer, 0, &sky.camera.matrix, 64);
        SDL_DispatchGPUCompute(compute_pass, groups_x, groups_y, 1);
        SDL_EndGPUComputePass(compute_pass);
        SDL_PopGPUDebugGroup(command_buffer);
    }
}

static void render_predepth(SDL_GPUCommandBuffer* command_buffer)
{
    SDL_GPUDepthStencilTargetInfo depth_info = {0};
    depth_info.load_op = SDL_GPU_LOADOP_LOAD;
    depth_info.store_op = SDL_GPU_STOREOP_STORE;
    depth_info.texture = depth_texture;
    SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command_buffer, NULL, 0, &depth_info);
    if (!render_pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    {
        world_pass_t data;
        data.mesh = WORLD_MESH_TRANSPARENT;
        data.camera = &player.camera;
        data.command_buffer = command_buffer;
        data.render_pass = render_pass;
        data.lights = false;
        SDL_BindGPUGraphicsPipeline(render_pass, predepth_pipeline);
        world_render(&data);
    }
    SDL_EndGPURenderPass(render_pass);
}

static void render_transparent(SDL_GPUCommandBuffer* command_buffer)
{
    SDL_GPUColorTargetInfo color_info = {0};
    color_info.load_op = SDL_GPU_LOADOP_LOAD;
    color_info.texture = composite_texture;
    color_info.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPUDepthStencilTargetInfo depth_info = {0};
    depth_info.load_op = SDL_GPU_LOADOP_LOAD;
    depth_info.store_op = SDL_GPU_STOREOP_STORE;
    depth_info.texture = depth_texture;
    SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command_buffer, &color_info, 1, &depth_info);
    if (!render_pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    {
        world_pass_t data;
        data.mesh = WORLD_MESH_TRANSPARENT;
        data.camera = &player.camera;
        data.command_buffer = command_buffer;
        data.render_pass = render_pass;
        data.lights = true;
        SDL_GPUTextureSamplerBinding atlas_binding = {atlas_texture, nearest_anisotropy_sampler};
        SDL_GPUTextureSamplerBinding shadow_binding = {shadow_texture, nearest_sampler};
        SDL_BindGPUGraphicsPipeline(render_pass, transparent_pipeline);
        SDL_PushGPUFragmentUniformData(command_buffer, 1, &sky.camera.matrix, 64);
        SDL_BindGPUFragmentSamplers(render_pass, 0, &atlas_binding, 1);
        SDL_BindGPUFragmentSamplers(render_pass, 1, &shadow_binding, 1);
        world_render(&data);
    }
    if (player.query.block != BLOCK_EMPTY)
    {
        SDL_PushGPUDebugGroup(command_buffer, "raycast");
        SDL_BindGPUGraphicsPipeline(render_pass, raycast_pipeline);
        SDL_PushGPUVertexUniformData(command_buffer, 0, player.camera.matrix, 64);
        SDL_PushGPUVertexUniformData(command_buffer, 1, player.query.current, 12);
        SDL_DrawGPUPrimitives(render_pass, 36, 1, 0, 0);
        SDL_PopGPUDebugGroup(command_buffer);
    }
    SDL_EndGPURenderPass(render_pass);
}

static void postprocess2(SDL_GPUCommandBuffer* command_buffer)
{
    {
        SDL_GPUStorageTextureReadWriteBinding write_textures = {fxaa_texture};
        SDL_GPUComputePass* compute_pass = SDL_BeginGPUComputePass(command_buffer, &write_textures, 1, NULL, 0);
        if (!compute_pass)
        {
            SDL_Log("Failed to begin compute pass: %s", SDL_GetError());
            return;
        }
        SDL_GPUTextureSamplerBinding read_samplers = {composite_texture, linear_sampler};
        int groups_x = (player.camera.width + 8 - 1) / 8;
        int groups_y = (player.camera.height + 8 - 1) / 8;
        SDL_PushGPUDebugGroup(command_buffer, "fxaa");
        SDL_BindGPUComputePipeline(compute_pass, fxaa_pipeline);
        SDL_BindGPUComputeSamplers(compute_pass, 0, &read_samplers, 1);
        SDL_DispatchGPUCompute(compute_pass, groups_x, groups_y, 1);
        SDL_EndGPUComputePass(compute_pass);
        SDL_PopGPUDebugGroup(command_buffer);
    }
    {
        SDL_GPUStorageTextureReadWriteBinding write_textures = {fxaa_texture};
        SDL_GPUComputePass* compute_pass = SDL_BeginGPUComputePass(command_buffer, &write_textures, 1, NULL, 0);
        if (!compute_pass)
        {
            SDL_Log("Failed to begin compute pass: %s", SDL_GetError());
            return;
        }
        SDL_GPUTextureSamplerBinding read_textures = {atlas_texture, nearest_sampler};
        Sint32 viewport[] = {player.camera.width, player.camera.height};
        Sint32 index = block_get_index(player.block, DIRECTION_NORTH);
        int groups_x = (player.camera.width + 8 - 1) / 8;
        int groups_y = (player.camera.height + 8 - 1) / 8;
        SDL_PushGPUDebugGroup(command_buffer, "ui");
        SDL_BindGPUComputePipeline(compute_pass, ui_pipeline);
        SDL_BindGPUComputeSamplers(compute_pass, 0, &read_textures, 1);
        SDL_PushGPUComputeUniformData(command_buffer, 0, viewport, sizeof(viewport));
        SDL_PushGPUComputeUniformData(command_buffer, 1, &index, sizeof(index));
        SDL_DispatchGPUCompute(compute_pass, groups_x, groups_y, 1);
        SDL_EndGPUComputePass(compute_pass);
        SDL_PopGPUDebugGroup(command_buffer);
    }
}

static void render()
{
    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(device);
    if (!command_buffer)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return;
    }
    SDL_GPUTexture* swapchain_texture;
    Uint32 width;
    Uint32 height;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, window, &swapchain_texture, &width, &height))
    {
        SDL_Log("Failed to acquire swapchain texture: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(command_buffer);
        return;
    }
    if (!swapchain_texture || !width || !height)
    {
        SDL_SubmitGPUCommandBuffer(command_buffer);
        return;
    }
    if ((width != player.camera.width || height != player.camera.height) && !resize(width, height))
    {
        SDL_SubmitGPUCommandBuffer(command_buffer);
        return;
    }
    camera_update(&player.camera);
    render_sky(command_buffer);
    render_geometry(command_buffer);
    postprocess(command_buffer);
    render_predepth(command_buffer);
    render_transparent(command_buffer);
    postprocess2(command_buffer);
    SDL_GPUBlitInfo info = {0};
    info.source.texture = fxaa_texture;
    info.source.w = player.camera.width;
    info.source.h = player.camera.height;
    info.destination.texture = swapchain_texture;
    info.destination.w = player.camera.width;
    info.destination.h = player.camera.height;
    info.load_op = SDL_GPU_LOADOP_DONT_CARE;
    info.filter = SDL_GPU_FILTER_NEAREST;
    SDL_BlitGPUTexture(command_buffer, &info);
    SDL_SubmitGPUCommandBuffer(command_buffer);
}

SDL_AppResult SDLCALL SDL_AppIterate(void* appstate)
{
    Uint64 ticks2 = SDL_GetTicks();
    float dt = ticks2 - ticks1;
    ticks1 = ticks2;
    if (SDL_GetWindowRelativeMouseMode(window))
    {
        player_move(&player, dt);
        player_save(&player);
    }
    sky_update(&sky, &player.camera, dt);
    world_update(&player.camera);
    render();
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDLCALL SDL_AppEvent(void* appstate, SDL_Event* event)
{
    switch (event->type)
    {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;
    case SDL_EVENT_MOUSE_MOTION:
        if (SDL_GetWindowRelativeMouseMode(window))
        {
            player_rotate(&player, event->motion.yrel, event->motion.xrel);
        }
        break;
    case SDL_EVENT_KEY_DOWN:
        if (event->key.scancode == SDL_SCANCODE_ESCAPE)
        {
            SDL_SetWindowRelativeMouseMode(window, false);
            SDL_SetWindowFullscreen(window, false);
        }
        else if (event->key.scancode == SDL_SCANCODE_F11)
        {
            if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN)
            {
                SDL_SetWindowFullscreen(window, false);
                SDL_SetWindowRelativeMouseMode(window, false);
            }
            else
            {
                SDL_SetWindowFullscreen(window, true);
                SDL_SetWindowRelativeMouseMode(window, true);
            }
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (!SDL_GetWindowRelativeMouseMode(window))
        {
            SDL_SetWindowRelativeMouseMode(window, true);
        }
        else
        {
            if (event->button.button == SDL_BUTTON_LEFT)
            {
                player_break(&player);
            }
            else if (event->button.button == SDL_BUTTON_RIGHT)
            {
                player_place(&player);
            }
        }
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        player_scroll(&player, event->wheel.y);
        break;
    }
    return SDL_APP_CONTINUE;
}
