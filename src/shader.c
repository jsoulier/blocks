#include <SDL3/SDL.h>
#include <jsmn.h>

#include "check.h"
#include "shader.h"

void* shader_load(SDL_GPUDevice* device, const char* name)
{
    SDL_GPUShaderFormat format = SDL_GetGPUShaderFormats(device);
    const char* entrypoint;
    const char* file_extension;
    if (format & SDL_GPU_SHADERFORMAT_SPIRV)
    {
        format = SDL_GPU_SHADERFORMAT_SPIRV;
        entrypoint = "main";
        file_extension = "spv";
    }
    else if (format & SDL_GPU_SHADERFORMAT_DXIL)
    {
        format = SDL_GPU_SHADERFORMAT_DXIL;
        entrypoint = "main";
        file_extension = "dxil";
    }
    else if (format & SDL_GPU_SHADERFORMAT_MSL)
    {
        format = SDL_GPU_SHADERFORMAT_MSL;
        entrypoint = "main0";
        file_extension = "msl";
    }
    else
    {
        CHECK(false);
    }
    char shader_path[512] = {0};
    char shader_json_path[512] = {0};
    SDL_snprintf(shader_path, sizeof(shader_path), "%s%s.%s", SDL_GetBasePath(), name, file_extension);
    SDL_snprintf(shader_json_path, sizeof(shader_json_path), "%s%s.json", SDL_GetBasePath(), name);
    size_t shader_size;
    size_t shader_json_size;
    char* shader_data = SDL_LoadFile(shader_path, &shader_size);
    if (!shader_data)
    {
        SDL_Log("Failed to load shader: %s", shader_path);
        return NULL;
    }
    char* shader_json_data = SDL_LoadFile(shader_json_path, &shader_json_size);
    if (!shader_json_data)
    {
        SDL_Log("Failed to load shader json: %s", shader_json_path);
        return NULL;
    }
    jsmn_parser json_parser;
    jsmntok_t json_tokens[128];
    jsmn_init(&json_parser);
    if (jsmn_parse(&json_parser, shader_json_data, shader_json_size, json_tokens, 128) <= 0)
    {
        SDL_Log("Failed to parse json: %s", shader_json_path);
        return NULL;
    }
    void* shader = NULL;
    if (SDL_strstr(name, ".comp"))
    {
        SDL_GPUComputePipelineCreateInfo info = {0};
        for (int i = 1; i < 128; i += 2)
        {
            if (json_tokens[i].type != JSMN_STRING)
            {
                continue;
            }
            char* key_string = shader_json_data + json_tokens[i + 0].start;
            char* value_string = shader_json_data + json_tokens[i + 1].start;
            int key_size = json_tokens[i + 0].end - json_tokens[i + 0].start;
            Uint32* value;
            if (!SDL_memcmp("samplers", key_string, key_size))
            {
                value = &info.num_samplers;
            }
            else if (!SDL_memcmp("readonly_storage_textures", key_string, key_size))
            {
                value = &info.num_readonly_storage_textures;
            }
            else if (!SDL_memcmp("readonly_storage_buffers", key_string, key_size))
            {
                value = &info.num_readonly_storage_buffers;
            }
            else if (!SDL_memcmp("readwrite_storage_textures", key_string, key_size))
            {
                value = &info.num_readwrite_storage_textures;
            }
            else if (!SDL_memcmp("readwrite_storage_buffers", key_string, key_size))
            {
                value = &info.num_readwrite_storage_buffers;
            }
            else if (!SDL_memcmp("uniform_buffers", key_string, key_size))
            {
                value = &info.num_uniform_buffers;
            }
            else if (!SDL_memcmp("threadcount_x", key_string, key_size))
            {
                value = &info.threadcount_x;
            }
            else if (!SDL_memcmp("threadcount_y", key_string, key_size))
            {
                value = &info.threadcount_y;
            }
            else if (!SDL_memcmp("threadcount_z", key_string, key_size))
            {
                value = &info.threadcount_z;
            }
            else
            {
                continue;
            }
            *value = *value_string - '0';
        }
        info.code = shader_data;
        info.code_size = shader_size;
        info.entrypoint = entrypoint;
        info.format = format;
        shader = SDL_CreateGPUComputePipeline(device, &info);
    }
    else
    {
        SDL_GPUShaderCreateInfo info = {0};
        for (int i = 1; i < 128; i += 2)
        {
            if (json_tokens[i].type != JSMN_STRING)
            {
                continue;
            }
            char* key_string = shader_json_data + json_tokens[i + 0].start;
            char* value_string = shader_json_data + json_tokens[i + 1].start;
            int key_size = json_tokens[i + 0].end - json_tokens[i + 0].start;
            Uint32* value;
            if (!SDL_memcmp("samplers", key_string, key_size))
            {
                value = &info.num_samplers;
            }
            else if (!SDL_memcmp("storage_textures", key_string, key_size))
            {
                value = &info.num_storage_textures;
            }
            else if (!SDL_memcmp("storage_buffers", key_string, key_size))
            {
                value = &info.num_storage_buffers;
            }
            else if (!SDL_memcmp("uniform_buffers", key_string, key_size))
            {
                value = &info.num_uniform_buffers;
            }
            else
            {
                continue;
            }
            *value = *value_string - '0';
        }
        info.code = shader_data;
        info.code_size = shader_size;
        info.entrypoint = entrypoint;
        info.format = format;
        if (SDL_strstr(name, ".frag"))
        {
            info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        }
        else
        {
            info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
        }
        shader = SDL_CreateGPUShader(device, &info);
    }
    if (!shader)
    {
        SDL_Log("Failed to create compute pipeline: %s", SDL_GetError());
        return NULL;
    }
    SDL_free(shader_data);
    SDL_free(shader_json_data);
    return shader;
}
