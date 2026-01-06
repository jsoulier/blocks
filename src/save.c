#include <SDL3/SDL.h>
#include <sqlite3.h>

#include "chunk.h"
#include "save.h"

static const int kPlayerID = 0;

static const char* kPlayerTable =
    "CREATE TABLE IF NOT EXISTS players ("
    "    id INT PRIMARY KEY NOT NULL,"
    "    x REAL NOT NULL,"
    "    y REAL NOT NULL,"
    "    z REAL NOT NULL,"
    "    pitch REAL NOT NULL,"
    "    yaw REAL NOT NULL,"
    "    roll REAL NOT NULL"
    ");";
static const char* kBlockTable =
    "CREATE TABLE IF NOT EXISTS blocks ("
    "    chunkX INTEGER NOT NULL,"
    "    chunkY INTEGER NOT NULL,"
    "    chunkZ INTEGER NOT NULL,"
    "    blockX INTEGER NOT NULL,"
    "    blockY INTEGER NOT NULL,"
    "    blockZ INTEGER NOT NULL,"
    "    block INTEGER NOT NULL,"
    "    PRIMARY KEY (chunkX, chunkY, chunkZ, blockX, blockY, blockZ)"
    ");";
static const char* kSetPlayer =
    "INSERT OR REPLACE INTO players (id, x, y, z, pitch, yaw, roll) "
    "VALUES (?, ?, ?, ?, ?, ?, ?);";
static const char* kGetPlayer =
    "SELECT x, y, z, pitch, yaw, roll FROM players "
    "WHERE id = ?;";
static const char* kSetBlock =
    "INSERT OR REPLACE INTO blocks (chunkX, chunkY, chunkZ, blockX, blockY, blockZ, block) "
    "VALUES (?, ?, ?, ?, ?, ?, ?);";
static const char* kGetChunk =
    "SELECT blockX, blockY, blockZ, block FROM blocks "
    "WHERE chunkX = ? AND chunkY = ? AND chunkZ = ?;";
static const char* kBlockIndex =
    "CREATE INDEX IF NOT EXISTS blocks_index "
    "ON blocks (chunkX, chunkY, chunkZ);";

bool CreateOrLoadSave(Save* save, const char* path)
{
    if (sqlite3_open(path, &save->Handle))
    {
        SDL_Log("Failed to open %s database: %s", path, sqlite3_errmsg(save->Handle));
        return false;
    }
    if (sqlite3_exec(save->Handle, kPlayerTable, NULL, NULL, NULL))
    {
        SDL_Log("Failed to create players table: %s", sqlite3_errmsg(save->Handle));
        return false;
    }
    if (sqlite3_exec(save->Handle, kBlockTable, NULL, NULL, NULL))
    {
        SDL_Log("Failed to create blocks table: %s", sqlite3_errmsg(save->Handle));
        return false;
    }
    if (sqlite3_prepare_v2(save->Handle, kSetPlayer, -1, &save->SetPlayerStatement, NULL))
    {
        SDL_Log("Failed to prepare set player: %s", sqlite3_errmsg(save->Handle));
        return false;
    }
    if (sqlite3_prepare_v2(save->Handle, kGetPlayer, -1, &save->GetPlayerStatement, NULL))
    {
        SDL_Log("Failed to prepare get player: %s", sqlite3_errmsg(save->Handle));
        return false;
    }
    if (sqlite3_prepare_v2(save->Handle, kSetBlock, -1, &save->SetBlockStatement, NULL))
    {
        SDL_Log("Failed to prepare set block: %s", sqlite3_errmsg(save->Handle));
        return false;
    }
    if (sqlite3_prepare_v2(save->Handle, kGetChunk, -1, &save->GetChunkStatement, NULL))
    {
        SDL_Log("Failed to prepare get blocks: %s", sqlite3_errmsg(save->Handle));
        return false;
    }
    if (sqlite3_exec(save->Handle, kBlockIndex, NULL, NULL, NULL))
    {
        SDL_Log("Failed to create blocks index: %s", sqlite3_errmsg(save->Handle));
        return false;
    }
    save->Mutex = SDL_CreateMutex();
    if (!save->Mutex)
    {
        SDL_Log("Failed to create mutex: %s", SDL_GetError());
        return false;
    }
    sqlite3_exec(save->Handle, "BEGIN;", NULL, NULL, NULL);
    return true;
}

void CloseSave(Save* save)
{
    SDL_DestroyMutex(save->Mutex);
    sqlite3_exec(save->Handle, "COMMIT;", NULL, NULL, NULL);
    sqlite3_finalize(save->SetPlayerStatement);
    sqlite3_finalize(save->GetPlayerStatement);
    sqlite3_finalize(save->SetBlockStatement);
    sqlite3_finalize(save->GetChunkStatement);
    sqlite3_close(save->Handle);
    save->Handle = NULL;
    save->SetPlayerStatement = NULL;
    save->GetPlayerStatement = NULL;
    save->SetBlockStatement = NULL;
    save->GetChunkStatement = NULL;
    save->Mutex = NULL;
}

void CommitSave(Save* save)
{
    if (!save->Handle)
    {
        return;
    }
    SDL_LockMutex(save->Mutex);
    sqlite3_exec(save->Handle, "COMMIT; BEGIN;", NULL, NULL, NULL);
    SDL_UnlockMutex(save->Mutex);
}

void SavePlayer(Save* save, float x, float y, float z, float pitch, float yaw, float roll)
{
    if (!save->Handle)
    {
        return;
    }
    SDL_LockMutex(save->Mutex);
    sqlite3_bind_int(save->SetPlayerStatement, 1, kPlayerID);
    sqlite3_bind_double(save->SetPlayerStatement, 2, x);
    sqlite3_bind_double(save->SetPlayerStatement, 3, y);
    sqlite3_bind_double(save->SetPlayerStatement, 4, z);
    sqlite3_bind_double(save->SetPlayerStatement, 5, pitch);
    sqlite3_bind_double(save->SetPlayerStatement, 6, yaw);
    sqlite3_bind_double(save->SetPlayerStatement, 7, roll);
    if (sqlite3_step(save->SetPlayerStatement) != SQLITE_DONE)
    {
        SDL_Log("Failed to set player: %s", sqlite3_errmsg(save->Handle));
    }
    sqlite3_reset(save->SetPlayerStatement);
    SDL_UnlockMutex(save->Mutex);
}

bool LoadPlayerFromSave(Save* save, float* x, float* y, float* z, float* pitch, float* yaw, float* roll)
{
    if (!save->Handle)
    {
        return false;
    }
    SDL_LockMutex(save->Mutex);
    sqlite3_bind_int(save->GetPlayerStatement, 1, kPlayerID);
    bool hasPlayer = sqlite3_step(save->GetPlayerStatement) == SQLITE_ROW;
    if (hasPlayer)
    {
        *x = sqlite3_column_double(save->GetPlayerStatement, 0);
        *y = sqlite3_column_double(save->GetPlayerStatement, 1);
        *z = sqlite3_column_double(save->GetPlayerStatement, 2);
        *pitch = sqlite3_column_double(save->GetPlayerStatement, 3);
        *yaw = sqlite3_column_double(save->GetPlayerStatement, 4);
        *roll = sqlite3_column_double(save->GetPlayerStatement, 5);
    }
    sqlite3_reset(save->GetPlayerStatement);
    SDL_UnlockMutex(save->Mutex);
    return hasPlayer;
}

void SaveBlock(Save* save, int chunkX, int chunkY, int chunkZ, int blockX, int blockY, int blockZ, Block block)
{
    if (!save->Handle)
    {
        return;
    }
    SDL_LockMutex(save->Mutex);
    sqlite3_bind_int(save->SetBlockStatement, 1, chunkX);
    sqlite3_bind_int(save->SetBlockStatement, 2, chunkY);
    sqlite3_bind_int(save->SetBlockStatement, 3, chunkZ);
    sqlite3_bind_int(save->SetBlockStatement, 4, blockX);
    sqlite3_bind_int(save->SetBlockStatement, 5, blockY);
    sqlite3_bind_int(save->SetBlockStatement, 6, blockZ);
    sqlite3_bind_int(save->SetBlockStatement, 7, block);
    if (sqlite3_step(save->SetBlockStatement) != SQLITE_DONE)
    {
        SDL_Log("Failed to set block: %s", sqlite3_errmsg(save->Handle));
    }
    sqlite3_reset(save->SetBlockStatement);
    SDL_UnlockMutex(save->Mutex);
}

void LoadChunkFromSave(Save* save, int chunkX, int chunkY, int chunkZ, Chunk* chunk)
{
    if (!save->Handle)
    {
        return;
    }
    SDL_LockMutex(save->Mutex);
    sqlite3_bind_int(save->GetChunkStatement, 1, chunkX);
    sqlite3_bind_int(save->GetChunkStatement, 2, chunkY);
    sqlite3_bind_int(save->GetChunkStatement, 3, chunkZ);
    while (sqlite3_step(save->GetChunkStatement) == SQLITE_ROW)
    {
        int x = sqlite3_column_int(save->GetChunkStatement, 0);
        int y = sqlite3_column_int(save->GetChunkStatement, 1);
        int z = sqlite3_column_int(save->GetChunkStatement, 2);
        Block block = sqlite3_column_int(save->GetChunkStatement, 3);
        SetChunkBlock(chunk, x, y, z, block);
    }
    sqlite3_reset(save->GetChunkStatement);
    SDL_UnlockMutex(save->Mutex);
}