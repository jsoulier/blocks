#pragma once

#include <SDL3/SDL.h>

#include "block.h"

typedef struct Chunk Chunk;
typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;

typedef struct Save
{
    sqlite3* Handle;
    sqlite3_stmt* SetPlayerStatement;
    sqlite3_stmt* GetPlayerStatement;
    sqlite3_stmt* SetBlockStatement;
    sqlite3_stmt* GetChunkStatement;
    SDL_Mutex* Mutex;
}
Save;

bool CreateOrLoadSave(Save* save, const char* path);
void CloseSave(Save* save);
void CommitSave(Save* save);
void SavePlayer(Save* save, float x, float y, float z, float pitch, float yaw, float roll);
bool LoadPlayerFromSave(Save* save, float* x, float* y, float* z, float* pitch, float* yaw, float* roll);
void SaveBlock(Save* save, int chunkX, int chunkY, int chunkZ, int blockX, int blockY, int blockZ, Block block);
void LoadChunkFromSave(Save* save, int chunkX, int chunkY, int chunkZ, Chunk* chunk);