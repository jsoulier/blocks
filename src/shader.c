#include <SDL3/SDL.h>
#include <jsmn.h>

#include "shader.h"

SDL_GPUShader* LoadShader(SDL_GPUDevice* device, const char* name)
{
    SDL_GPUShaderFormat format = SDL_GetGPUShaderFormats(device);
    const char* entrypoint;
    const char* fileExtension;
    if (format & SDL_GPU_SHADERFORMAT_SPIRV)
    {
        format = SDL_GPU_SHADERFORMAT_SPIRV;
        entrypoint = "main";
        fileExtension = "spv";
    }
    else if (format & SDL_GPU_SHADERFORMAT_DXIL)
    {
        format = SDL_GPU_SHADERFORMAT_DXIL;
        entrypoint = "main";
        fileExtension = "dxil";
    }
    else if (format & SDL_GPU_SHADERFORMAT_MSL)
    {
        format = SDL_GPU_SHADERFORMAT_MSL;
        entrypoint = "main0";
        fileExtension = "msl";
    }
    else
    {
        SDL_assert(false);
    }
    char shaderPath[512] = {0};
    char shaderJsonPath[512] = {0};
    SDL_snprintf(shaderPath, sizeof(shaderPath), "%s%s.%s", SDL_GetBasePath(), name, fileExtension);
    SDL_snprintf(shaderJsonPath, sizeof(shaderJsonPath), "%s%s.json", SDL_GetBasePath(), name);
    size_t shaderSize;
    size_t shaderJsonSize;
    char* shaderData = SDL_LoadFile(shaderPath, &shaderSize);
    if (!shaderData)
    {
        SDL_Log("Failed to load shader: %s", shaderPath);
        return NULL;
    }
    char* shaderJsonData = SDL_LoadFile(shaderJsonPath, &shaderJsonSize);
    if (!shaderJsonData)
    {
        SDL_Log("Failed to load shader json: %s", shaderJsonPath);
        return NULL;
    }
    jsmn_parser jsonParser;
    jsmntok_t jsonTokens[64];
    jsmn_init(&jsonParser);
    if (jsmn_parse(&jsonParser, shaderJsonData, shaderJsonSize, jsonTokens, 64) <= 0)
    {
        SDL_Log("Failed to parse json: %s", shaderJsonPath);
        return NULL;
    }
    SDL_GPUShaderCreateInfo info = {0};
    for (int i = 1; i < 64; i += 2)
    {
        if (jsonTokens[i].type != JSMN_STRING)
        {
            continue;
        }
        char* keyString = shaderJsonData + jsonTokens[i + 0].start;
        char* valueString = shaderJsonData + jsonTokens[i + 1].start;
        int keySize = jsonTokens[i + 0].end - jsonTokens[i + 0].start;
        Uint32* value;
        if (!SDL_memcmp("samplers", keyString, keySize))
        {
            value = &info.num_samplers;
        }
        else if (!SDL_memcmp("storage_textures", keyString, keySize))
        {
            value = &info.num_storage_textures;
        }
        else if (!SDL_memcmp("storage_buffers", keyString, keySize))
        {
            value = &info.num_storage_buffers;
        }
        else if (!SDL_memcmp("uniform_buffers", keyString, keySize))
        {
            value = &info.num_uniform_buffers;
        }
        else
        {
            continue;
        }
        *value = *valueString - '0';
    }
    info.code = shaderData;
    info.code_size = shaderSize;
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
    SDL_GPUShader* shader = SDL_CreateGPUShader(device, &info);
    if (!shader)
    {
        SDL_Log("Failed to create shader: %s", SDL_GetError());
        return NULL;
    }
    SDL_free(shaderData);
    SDL_free(shaderJsonData);
    return shader;
}

SDL_GPUComputePipeline* LoadComputePipeline(SDL_GPUDevice* device, const char* name)
{
    SDL_GPUShaderFormat format = SDL_GetGPUShaderFormats(device);
    const char* entrypoint;
    const char* fileExtension;
    if (format & SDL_GPU_SHADERFORMAT_SPIRV)
    {
        format = SDL_GPU_SHADERFORMAT_SPIRV;
        entrypoint = "main";
        fileExtension = "spv";
    }
    else if (format & SDL_GPU_SHADERFORMAT_DXIL)
    {
        format = SDL_GPU_SHADERFORMAT_DXIL;
        entrypoint = "main";
        fileExtension = "dxil";
    }
    else if (format & SDL_GPU_SHADERFORMAT_MSL)
    {
        format = SDL_GPU_SHADERFORMAT_MSL;
        entrypoint = "main0";
        fileExtension = "msl";
    }
    else
    {
        SDL_assert(false);
    }
    char shaderPath[512] = {0};
    char shaderJsonPath[512] = {0};
    SDL_snprintf(shaderPath, sizeof(shaderPath), "%s%s.%s", SDL_GetBasePath(), name, fileExtension);
    SDL_snprintf(shaderJsonPath, sizeof(shaderJsonPath), "%s%s.json", SDL_GetBasePath(), name);
    size_t shaderSize;
    size_t shaderJsonSize;
    char* shaderData = SDL_LoadFile(shaderPath, &shaderSize);
    if (!shaderData)
    {
        SDL_Log("Failed to load shader: %s", shaderPath);
        return NULL;
    }
    char* shaderJsonData = SDL_LoadFile(shaderJsonPath, &shaderJsonSize);
    if (!shaderJsonData)
    {
        SDL_Log("Failed to load shader json: %s", shaderJsonPath);
        return NULL;
    }
    jsmn_parser jsonParser;
    jsmntok_t jsonTokens[64];
    jsmn_init(&jsonParser);
    if (jsmn_parse(&jsonParser, shaderJsonData, shaderJsonSize, jsonTokens, 64) <= 0)
    {
        SDL_Log("Failed to parse json: %s", shaderJsonPath);
        return NULL;
    }
    SDL_GPUComputePipelineCreateInfo info = {0};
    for (int i = 1; i < 64; i += 2)
    {
        if (jsonTokens[i].type != JSMN_STRING)
        {
            continue;
        }
        char* keyString = shaderJsonData + jsonTokens[i + 0].start;
        char* valueString = shaderJsonData + jsonTokens[i + 1].start;
        int keySize = jsonTokens[i + 0].end - jsonTokens[i + 0].start;
        Uint32* value;
        if (!SDL_memcmp("samplers", keyString, keySize))
        {
            value = &info.num_samplers;
        }
        else if (!SDL_memcmp("readonly_storage_textures", keyString, keySize))
        {
            value = &info.num_readonly_storage_textures;
        }
        else if (!SDL_memcmp("readonly_storage_buffers", keyString, keySize))
        {
            value = &info.num_readonly_storage_buffers;
        }
        else if (!SDL_memcmp("readwrite_storage_textures", keyString, keySize))
        {
            value = &info.num_readwrite_storage_textures;
        }
        else if (!SDL_memcmp("readwrite_storage_buffers", keyString, keySize))
        {
            value = &info.num_readwrite_storage_buffers;
        }
        else if (!SDL_memcmp("uniform_buffers", keyString, keySize))
        {
            value = &info.num_uniform_buffers;
        }
        else if (!SDL_memcmp("threadcount_x", keyString, keySize))
        {
            value = &info.threadcount_x;
        }
        else if (!SDL_memcmp("threadcount_y", keyString, keySize))
        {
            value = &info.threadcount_y;
        }
        else if (!SDL_memcmp("threadcount_z", keyString, keySize))
        {
            value = &info.threadcount_z;
        }
        else
        {
            continue;
        }
        *value = *valueString - '0';
    }
    info.code = shaderData;
    info.code_size = shaderSize;
    info.entrypoint = entrypoint;
    info.format = format;
    SDL_GPUComputePipeline* pipeline = SDL_CreateGPUComputePipeline(device, &info);
    if (!pipeline)
    {
        SDL_Log("Failed to create compute pipeline: %s", SDL_GetError());
        return NULL;
    }
    SDL_free(shaderData);
    SDL_free(shaderJsonData);
    return pipeline;
}