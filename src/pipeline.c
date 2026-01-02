#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "helpers.h"
#include "pipeline.h"

static SDL_GPUDevice* device;
static SDL_GPUGraphicsPipeline* pipelines[PIPELINE_COUNT];

static SDL_GPUShader* load(
    const char* file,
    const int uniforms,
    const int samplers)
{
    assert(file);
    SDL_GPUShaderCreateInfo info = {0};
    const char* extension = NULL;
    if (SDL_GetGPUShaderFormats(device) & SDL_GPU_SHADERFORMAT_SPIRV)
    {
        info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        info.entrypoint = "main";
        extension = "spv";
    }
    else if (SDL_GetGPUShaderFormats(device) & SDL_GPU_SHADERFORMAT_MSL)
    {
        info.format = SDL_GPU_SHADERFORMAT_MSL;
        info.entrypoint = "main0";
        extension = "msl";
    }
    else
    {
        SDL_Log("Unknown shader format");
        return NULL;
    }
    char path[512] = {0};
    snprintf(path, sizeof(path), "%s%s.%s", SDL_GetBasePath(), file, extension);
    void* code = SDL_LoadFile(path, &info.code_size);
    if (!code)
    {
        SDL_Log("Failed to load %s shader: %s", file, SDL_GetError());
        return NULL;
    }
    info.code = code;
    if (strstr(file, ".vert"))
    {
        info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
    }
    else
    {
        info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    }
    info.num_uniform_buffers = uniforms;
    info.num_samplers = samplers;
    SDL_GPUShader* shader = SDL_CreateGPUShader(device, &info);
    SDL_free(code);
    if (!shader)
    {
        SDL_Log("Failed to create %s shader: %s", file, SDL_GetError());
        return NULL;
    }
    return shader;
}

static SDL_GPUGraphicsPipeline* load_sky(
    const SDL_GPUTextureFormat format)
{
    SDL_GPUGraphicsPipelineCreateInfo info =
    {
        .vertex_shader = load("sky.vert", 2, 0),
        .fragment_shader = load("sky.frag", 0, 0),
        .target_info =
        {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[])
            {{
                .format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
            }},
        },
        .vertex_input_state =
        {
            .num_vertex_attributes = 1,
            .vertex_attributes = (SDL_GPUVertexAttribute[])
            {{
                .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
            }},
            .num_vertex_buffers = 1,
            .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[])
            {{
                .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                .pitch = 12,
            }},
        },
    };
    SDL_GPUGraphicsPipeline* pipeline = NULL;
    if (info.vertex_shader && info.fragment_shader)
    {
        pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    if (!pipeline)
    {
        SDL_Log("Failed to create sky pipeline: %s", SDL_GetError());
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return pipeline;
}

static SDL_GPUGraphicsPipeline* load_shadow(
    const SDL_GPUTextureFormat format)
{
    SDL_GPUGraphicsPipelineCreateInfo info =
    {
        .vertex_shader = load("shadow.vert", 2, 0),
        .fragment_shader = load("shadow.frag", 0, 0),
        .target_info =
        {
            .has_depth_stencil_target = true,
            .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
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
    SDL_GPUGraphicsPipeline* pipeline = NULL;
    if (info.vertex_shader && info.fragment_shader)
    {
        pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    if (!pipeline)
    {
        SDL_Log("Failed to create shadow pipeline: %s", SDL_GetError());
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return pipeline;
}

static SDL_GPUGraphicsPipeline* load_opaque(
    const SDL_GPUTextureFormat format)
{
    SDL_GPUGraphicsPipelineCreateInfo info =
    {
        .vertex_shader = load("opaque.vert", 3, 0),
        .fragment_shader = load("opaque.frag", 0, 1),
        .target_info =
        {
            .num_color_targets = 3,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[])
            {{
                .format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
            },
            {
                .format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
            },
            {
                .format = SDL_GPU_TEXTUREFORMAT_R32_UINT,
            }},
            .has_depth_stencil_target = true,
            .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
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
    SDL_GPUGraphicsPipeline* pipeline = NULL;
    if (info.vertex_shader && info.fragment_shader)
    {
        pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    if (!pipeline)
    {
        SDL_Log("Failed to create opaque pipeline: %s", SDL_GetError());
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return pipeline;
}

static SDL_GPUGraphicsPipeline* load_ssao(
    const SDL_GPUTextureFormat format)
{
    SDL_GPUGraphicsPipelineCreateInfo info =
    {
        .vertex_shader = load("fullscreen.vert", 0, 0),
        .fragment_shader = load("ssao.frag", 0, 4),
        .target_info =
        {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[])
            {{
                .format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT
            }}
        },
    };
    SDL_GPUGraphicsPipeline* pipeline = NULL;
    if (info.vertex_shader && info.fragment_shader)
    {
        pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    if (!pipeline)
    {
        SDL_Log("Failed to create ssao pipeline: %s", SDL_GetError());
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return pipeline;
}

static SDL_GPUGraphicsPipeline* load_composite(
    const SDL_GPUTextureFormat format)
{
    SDL_GPUGraphicsPipelineCreateInfo info =
    {
        .vertex_shader = load("fullscreen.vert", 0, 0),
        .fragment_shader = load("composite.frag", 3, 6),
        .target_info =
        {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[])
            {{
                .format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
            }},
        },
    };
    SDL_GPUGraphicsPipeline* pipeline = NULL;
    if (info.vertex_shader && info.fragment_shader)
    {
        pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    if (!pipeline)
    {
        SDL_Log("Failed to create composite pipeline: %s", SDL_GetError());
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return pipeline;
}

static SDL_GPUGraphicsPipeline* load_transparent(
    const SDL_GPUTextureFormat format)
{
    SDL_GPUGraphicsPipelineCreateInfo info =
    {
        .vertex_shader = load("transparent.vert", 4, 0),
        .fragment_shader = load("transparent.frag", 4, 3),
        .target_info =
        {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[])
            {{
                .format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
                .blend_state =
                {
                    .enable_blend = true,
                    .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                    .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                    .src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
                    .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                    .color_blend_op = SDL_GPU_BLENDOP_ADD,
                    .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
                },
            }},
            .has_depth_stencil_target = true,
            .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
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
            .enable_depth_write = false,
            .compare_op = SDL_GPU_COMPAREOP_LESS,
        },
    };
    SDL_GPUGraphicsPipeline* pipeline = NULL;
    if (info.vertex_shader && info.fragment_shader)
    {
        pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    if (!pipeline)
    {
        SDL_Log("Failed to create transparent pipeline: %s", SDL_GetError());
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return pipeline;
}

static SDL_GPUGraphicsPipeline* load_raycast(
    const SDL_GPUTextureFormat format)
{
    SDL_GPUGraphicsPipelineCreateInfo info =
    {
        .vertex_shader = load("raycast.vert", 2, 0),
        .fragment_shader = load("raycast.frag", 0, 0),
        .target_info =
        {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[])
            {{
                .format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
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
            .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        },
        .vertex_input_state =
        {
            .num_vertex_attributes = 1,
            .vertex_attributes = (SDL_GPUVertexAttribute[])
            {{
                .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
            }},
            .num_vertex_buffers = 1,
            .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[])
            {{
                .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                .pitch = 12,
            }},
        },
        .depth_stencil_state =
        {
            .enable_depth_test = true,
            .enable_depth_write = true,
            .compare_op = SDL_GPU_COMPAREOP_LESS,
        },
    };
    SDL_GPUGraphicsPipeline* pipeline = NULL;
    if (info.vertex_shader && info.fragment_shader)
    {
        pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    if (!pipeline)
    {
        SDL_Log("Failed to create raycast pipeline: %s", SDL_GetError());
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return pipeline;
}

static SDL_GPUGraphicsPipeline* load_ui(
    const SDL_GPUTextureFormat format)
{
    SDL_GPUGraphicsPipelineCreateInfo info =
    {
        .vertex_shader = load("fullscreen.vert", 0, 0),
        .fragment_shader = load("ui.frag", 2, 1),
        .target_info =
        {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[])
            {{
                .format = format,
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
        },
    };
    SDL_GPUGraphicsPipeline* pipeline = NULL;
    if (info.vertex_shader && info.fragment_shader)
    {
        pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    if (!pipeline)
    {
        SDL_Log("Failed to create ui pipeline: %s", SDL_GetError());
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return pipeline;
}

static SDL_GPUGraphicsPipeline* load_random(
    const SDL_GPUTextureFormat format)
{
    SDL_GPUGraphicsPipelineCreateInfo info =
    {
        .vertex_shader = load("fullscreen.vert", 0, 0),
        .fragment_shader = load("random.frag", 0, 0),
        .target_info =
        {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[])
            {{
                .format = SDL_GPU_TEXTUREFORMAT_R32G32_FLOAT,
            }},
        },
    };
    SDL_GPUGraphicsPipeline* pipeline = NULL;
    if (info.vertex_shader && info.fragment_shader)
    {
        pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    if (!pipeline)
    {
        SDL_Log("Failed to create ui pipeline: %s", SDL_GetError());
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return pipeline;
}

bool pipeline_init(
    SDL_GPUDevice* handle,
    const SDL_GPUTextureFormat format)
{
    assert(handle);
    assert(format);
    device = handle;
    pipelines[PIPELINE_SKY] = load_sky(format);
    pipelines[PIPELINE_SHADOW] = load_shadow(format);
    pipelines[PIPELINE_OPAQUE] = load_opaque(format);
    pipelines[PIPELINE_SSAO] = load_ssao(format);
    pipelines[PIPELINE_COMPOSITE] = load_composite(format);
    pipelines[PIPELINE_TRANSPARENT] = load_transparent(format);
    pipelines[PIPELINE_RAYCAST] = load_raycast(format);
    pipelines[PIPELINE_UI] = load_ui(format);
    pipelines[PIPELINE_RANDOM] = load_random(format);
    for (pipeline_t pipeline = 0; pipeline < PIPELINE_COUNT; pipeline++)
    {
        if (!pipelines[pipeline])
        {
            SDL_Log("Failed to load pipeline: %d", pipeline);
            return false;
        }
    }
    return true;
}

void pipeline_free()
{
    for (pipeline_t pipeline = 0; pipeline < PIPELINE_COUNT; pipeline++)
    {
        if (pipelines[pipeline])
        {
            SDL_ReleaseGPUGraphicsPipeline(device, pipelines[pipeline]);
            pipelines[pipeline] = NULL;
        }
    }
    device = NULL;
}

void pipeline_bind(
    void* pass,
    const pipeline_t pipeline)
{
    assert(pass);
    assert(pipeline < PIPELINE_COUNT);
    switch (pipeline)
    {
    case PIPELINE_SKY:
    case PIPELINE_SHADOW:
    case PIPELINE_OPAQUE:
    case PIPELINE_SSAO:
    case PIPELINE_COMPOSITE:
    case PIPELINE_TRANSPARENT:
    case PIPELINE_RAYCAST:
    case PIPELINE_UI:
    case PIPELINE_RANDOM:
        SDL_BindGPUGraphicsPipeline(pass, pipelines[pipeline]);
        break;
    default:
        assert(0);
    }
}