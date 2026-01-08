#include <SDL3/SDL.h>
#include <sqlite3.h>

#include "camera.h"
#include "chunk.h"
#include "player.h"
#include "save.h"

static const char* PLAYERS =
    "CREATE TABLE IF NOT EXISTS players ("
    "    id INT PRIMARY KEY NOT NULL,"
    "    x REAL NOT NULL,"
    "    y REAL NOT NULL,"
    "    z REAL NOT NULL,"
    "    pitch REAL NOT NULL,"
    "    yaw REAL NOT NULL,"
    "    roll REAL NOT NULL,"
    "    block INT NOT NULL"
    ");";
static const char* BLOCKS =
    "CREATE TABLE IF NOT EXISTS blocks ("
    "    chunk_x INTEGER NOT NULL,"
    "    chunk_z INTEGER NOT NULL,"
    "    block_x INTEGER NOT NULL,"
    "    block_y INTEGER NOT NULL,"
    "    block_z INTEGER NOT NULL,"
    "    block INTEGER NOT NULL,"
    "    PRIMARY KEY (chunk_x, chunk_z, block_x, block_y, block_z)"
    ");";
static const char* SET_PLAYER = "INSERT OR REPLACE INTO players (id, x, y, z, pitch, yaw, roll, block) VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
static const char* GET_PLAYER = "SELECT x, y, z, pitch, yaw, roll, block FROM players WHERE id = ?;";
static const char* SET_BLOCK = "INSERT OR REPLACE INTO blocks (chunk_x, chunk_z, block_x, block_y, block_z, block) VALUES (?, ?, ?, ?, ?, ?);";
static const char* GET_CHUNK = "SELECT block_x, block_y, block_z, block FROM blocks WHERE chunk_x = ? AND chunk_z = ?;";
static const char* BLOCK_INDEX = "CREATE INDEX IF NOT EXISTS block_index ON blocks (chunk_x, chunk_z);";

static sqlite3* handle;
static sqlite3_stmt* set_player;
static sqlite3_stmt* get_player;
static sqlite3_stmt* set_block;
static sqlite3_stmt* get_chunk;
static SDL_Mutex* mutex;

bool save_init(const char* path)
{
    if (sqlite3_open(path, &handle))
    {
        SDL_Log("Failed to open %s database: %s", path, sqlite3_errmsg(handle));
        return false;
    }
    if (sqlite3_exec(handle, PLAYERS, NULL, NULL, NULL))
    {
        SDL_Log("Failed to create players: %s", sqlite3_errmsg(handle));
        return false;
    }
    if (sqlite3_exec(handle, BLOCKS, NULL, NULL, NULL))
    {
        SDL_Log("Failed to create blocks: %s", sqlite3_errmsg(handle));
        return false;
    }
    if (sqlite3_prepare_v2(handle, SET_PLAYER, -1, &set_player, NULL))
    {
        SDL_Log("Failed to prepare set player: %s", sqlite3_errmsg(handle));
        return false;
    }
    if (sqlite3_prepare_v2(handle, GET_PLAYER, -1, &get_player, NULL))
    {
        SDL_Log("Failed to prepare get player: %s", sqlite3_errmsg(handle));
        return false;
    }
    if (sqlite3_prepare_v2(handle, SET_BLOCK, -1, &set_block, NULL))
    {
        SDL_Log("Failed to prepare set block: %s", sqlite3_errmsg(handle));
        return false;
    }
    if (sqlite3_prepare_v2(handle, GET_CHUNK, -1, &get_chunk, NULL))
    {
        SDL_Log("Failed to prepare get blocks: %s", sqlite3_errmsg(handle));
        return false;
    }
    if (sqlite3_exec(handle, BLOCK_INDEX, NULL, NULL, NULL))
    {
        SDL_Log("Failed to create block index: %s", sqlite3_errmsg(handle));
        return false;
    }
    mutex = SDL_CreateMutex();
    if (!mutex)
    {
        SDL_Log("Failed to create mutex: %s", SDL_GetError());
        return false;
    }
    sqlite3_exec(handle, "BEGIN;", NULL, NULL, NULL);
    return true;
}

void save_free()
{
    SDL_DestroyMutex(mutex);
    sqlite3_exec(handle, "COMMIT;", NULL, NULL, NULL);
    sqlite3_finalize(set_player);
    sqlite3_finalize(get_player);
    sqlite3_finalize(set_block);
    sqlite3_finalize(get_chunk);
    sqlite3_close(handle);
    handle = NULL;
    set_player = NULL;
    get_player = NULL;
    set_block = NULL;
    get_chunk = NULL;
    mutex = NULL;
}

void save_commit()
{
    if (!handle)
    {
        return;
    }
    SDL_LockMutex(mutex);
    sqlite3_exec(handle, "COMMIT; BEGIN;", NULL, NULL, NULL);
    SDL_UnlockMutex(mutex);
}

void save_set_player(const player_t* player)
{
    if (!handle)
    {
        return;
    }
    float x;
    float y;
    float z;
    float pitch;
    float roll;
    float yaw;
    camera_get_position(&player->camera, &x, &y, &z);
    camera_get_rotation(&player->camera, &pitch, &yaw, &roll);
    SDL_LockMutex(mutex);
    sqlite3_bind_int(set_player, 1, player->id);
    sqlite3_bind_double(set_player, 2, x);
    sqlite3_bind_double(set_player, 3, y);
    sqlite3_bind_double(set_player, 4, z);
    sqlite3_bind_double(set_player, 5, pitch);
    sqlite3_bind_double(set_player, 6, yaw);
    sqlite3_bind_double(set_player, 7, roll);
    sqlite3_bind_int(set_player, 8, player->block);
    if (sqlite3_step(set_player) != SQLITE_DONE)
    {
        SDL_Log("Failed to set player: %s", sqlite3_errmsg(handle));
    }
    sqlite3_reset(set_player);
    SDL_UnlockMutex(mutex);
}

bool save_get_player(player_t* player)
{
    if (!handle)
    {
        return false;
    }
    SDL_LockMutex(mutex);
    sqlite3_bind_int(get_player, 1, player->id);
    bool has_player = sqlite3_step(get_player) == SQLITE_ROW;
    if (has_player)
    {
        float x = sqlite3_column_double(get_player, 0);
        float y = sqlite3_column_double(get_player, 1);
        float z = sqlite3_column_double(get_player, 2);
        float pitch = sqlite3_column_double(get_player, 3);
        float yaw = sqlite3_column_double(get_player, 4);
        float roll = sqlite3_column_double(get_player, 5);
        player->block = sqlite3_column_int(get_player, 6);
        camera_set_position(&player->camera, x, y, z);
        camera_set_rotation(&player->camera, pitch, yaw, roll);
    }
    sqlite3_reset(get_player);
    SDL_UnlockMutex(mutex);
    return has_player;
}

void save_set_block(const chunk_t* chunk, int x, int y, int z, block_t block)
{
    if (!handle)
    {
        return;
    }
    SDL_LockMutex(mutex);
    sqlite3_bind_int(set_block, 1, chunk->x);
    sqlite3_bind_int(set_block, 2, chunk->z);
    sqlite3_bind_int(set_block, 3, x);
    sqlite3_bind_int(set_block, 4, y);
    sqlite3_bind_int(set_block, 5, z);
    sqlite3_bind_int(set_block, 6, block);
    if (sqlite3_step(set_block) != SQLITE_DONE)
    {
        SDL_Log("Failed to set block: %s", sqlite3_errmsg(handle));
    }
    sqlite3_reset(set_block);
    SDL_UnlockMutex(mutex);
}

void save_get_chunk(chunk_t* chunk)
{
    if (!handle)
    {
        return;
    }
    SDL_LockMutex(mutex);
    sqlite3_bind_int(get_chunk, 1, chunk->x);
    sqlite3_bind_int(get_chunk, 2, chunk->z);
    while (sqlite3_step(get_chunk) == SQLITE_ROW)
    {
        int x = sqlite3_column_int(get_chunk, 0);
        int y = sqlite3_column_int(get_chunk, 1);
        int z = sqlite3_column_int(get_chunk, 2);
        block_t block = sqlite3_column_int(get_chunk, 3);
        chunk_set_block(chunk, x, y, z, block);
    }
    sqlite3_reset(get_chunk);
    SDL_UnlockMutex(mutex);
}
