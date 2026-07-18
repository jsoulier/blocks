#include <SDL3/SDL.h>
#include <jsmn.h>

#include "shader.h"

static bool TokenIs(const char* json, const jsmntok_t* token, const char* string)
{
    int size = token->end - token->start;
    return size == SDL_strlen(string) && !SDL_memcmp(json + token->start, string, size);
}

static Uint32 TokenGetUint(const char* json, const jsmntok_t* token)
{
    Uint32 value = 0;
    for (int i = token->start; i < token->end; i++)
    {
        SDL_assert(json[i] >= '0' && json[i] <= '9');
        value = value * 10 + json[i] - '0';
    }
    return value;
}

static bool GetResources(const char* json, size_t size, SDL_GPUShaderCreateInfo* info)
{
    jsmn_parser parser;
    jsmntok_t tokens[128] = {0};
    jsmn_init(&parser);
    int count = jsmn_parse(&parser, json, size, tokens, SDL_arraysize(tokens));
    if (count <= 0)
    {
        return false;
    }
    for (int i = 1; i + 1 < count; i += 2)
    {
        if (tokens[i].type != JSMN_STRING)
        {
            continue;
        }
        Uint32 value = TokenGetUint(json, &tokens[i + 1]);
        if (TokenIs(json, &tokens[i], "samplers"))
        {
            info->num_samplers = value;
        }
        else if (TokenIs(json, &tokens[i], "storage_textures"))
        {
            info->num_storage_textures = value;
        }
        else if (TokenIs(json, &tokens[i], "storage_buffers"))
        {
            info->num_storage_buffers = value;
        }
        else if (TokenIs(json, &tokens[i], "uniform_buffers"))
        {
            info->num_uniform_buffers = value;
        }
    }
    return true;
}

SDL_GPUShader* Shader_Load(SDL_GPUDevice* device, const char* name)
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
        SDL_assert(false);
        SDL_Log("Unsupported shader format");
        return NULL;
    }
    char shader_path[512] = {0};
    char shader_json_path[512] = {0};
    SDL_snprintf(shader_path, sizeof(shader_path), "%s%s.%s", SDL_GetBasePath(), name, file_extension);
    SDL_snprintf(shader_json_path, sizeof(shader_json_path), "%s%s.json", SDL_GetBasePath(), name);
    size_t shader_size;
    size_t shader_json_size;
    Uint8* shader_data = SDL_LoadFile(shader_path, &shader_size);
    char* shader_json_data = NULL;
    SDL_GPUShader* shader = NULL;
    if (!shader_data)
    {
        SDL_Log("Failed to load shader: %s", shader_path);
        return NULL;
    }
    shader_json_data = SDL_LoadFile(shader_json_path, &shader_json_size);
    if (!shader_json_data)
    {
        SDL_Log("Failed to load shader json: %s", shader_json_path);
        goto cleanup;
    }
    SDL_GPUShaderCreateInfo info = {0};
    if (!GetResources(shader_json_data, shader_json_size, &info))
    {
        SDL_Log("Failed to parse json: %s", shader_json_path);
        goto cleanup;
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
    if (!shader)
    {
        SDL_Log("Failed to create shader: %s", SDL_GetError());
    }
cleanup:
    SDL_free(shader_data);
    SDL_free(shader_json_data);
    return shader;
}
