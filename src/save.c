#include <SDL3/SDL.h>
#include <sqlite3.h>

#include "save.h"

static const char* PLAYER_TABLE =
    "CREATE TABLE IF NOT EXISTS players ("
    "    id INT PRIMARY KEY NOT NULL,"
    "    data BLOB NOT NULL"
    ");";
static const char* BLOCK_TABLE =
    "CREATE TABLE IF NOT EXISTS blocks ("
    "    cx INTEGER NOT NULL,"
    "    cz INTEGER NOT NULL,"
    "    bx INTEGER NOT NULL,"
    "    by INTEGER NOT NULL,"
    "    bz INTEGER NOT NULL,"
    "    block INTEGER NOT NULL,"
    "    PRIMARY KEY (cx, cz, bx, by, bz)"
    ");";
static const char* SET_PLAYER = "INSERT OR REPLACE INTO players (id, data) VALUES (?, ?);";
static const char* GET_PLAYER = "SELECT data FROM players WHERE id = ?;";
static const char* SET_BLOCK = "INSERT OR REPLACE INTO blocks (cx, cz, bx, by, bz, block) VALUES (?, ?, ?, ?, ?, ?);";
static const char* GET_BLOCKS = "SELECT bx, by, bz, block FROM blocks WHERE cx = ? AND cz = ?;";
static const char* BLOCK_INDEX = "CREATE INDEX IF NOT EXISTS bindex ON blocks (cx, cz);";

static sqlite3* handle;
static sqlite3_stmt* set_player;
static sqlite3_stmt* get_player;
static sqlite3_stmt* set_block;
static sqlite3_stmt* get_blocks;
static SDL_Mutex* mutex;

bool save_init(const char* path)
{
    if (sqlite3_open(path, &handle))
    {
        SDL_Log("Failed to open %s database: %s", path, sqlite3_errmsg(handle));
        return false;
    }
    if (sqlite3_exec(handle, PLAYER_TABLE, NULL, NULL, NULL))
    {
        SDL_Log("Failed to create player table: %s", sqlite3_errmsg(handle));
        return false;
    }
    if (sqlite3_exec(handle, BLOCK_TABLE, NULL, NULL, NULL))
    {
        SDL_Log("Failed to create block table: %s", sqlite3_errmsg(handle));
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
    if (sqlite3_prepare_v2(handle, GET_BLOCKS, -1, &get_blocks, NULL))
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
    sqlite3_finalize(get_blocks);
    sqlite3_close(handle);
    handle = NULL;
    set_player = NULL;
    get_player = NULL;
    set_block = NULL;
    get_blocks = NULL;
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

void save_set_player(int id, const void* data, int size)
{
    if (!handle)
    {
        return;
    }
    SDL_LockMutex(mutex);
    sqlite3_bind_int(set_player, 1, id);
    sqlite3_bind_blob(set_player, 2, data, size, SQLITE_TRANSIENT);
    if (sqlite3_step(set_player) != SQLITE_DONE)
    {
        SDL_Log("Failed to set player: %s", sqlite3_errmsg(handle));
    }
    sqlite3_reset(set_player);
    SDL_UnlockMutex(mutex);
}

bool save_get_player(int id, void* data, int size)
{
    if (!handle)
    {
        return false;
    }
    SDL_LockMutex(mutex);
    sqlite3_bind_int(get_player, 1, id);
    bool has_player = sqlite3_step(get_player) == SQLITE_ROW;
    if (has_player)
    {
        const void* out_data = sqlite3_column_blob(get_player, 0);
        int out_size = sqlite3_column_bytes(get_player, 0);
        if (size == out_size)
        {
            SDL_memcpy(data, out_data, size);
        }
        else
        {
            SDL_Log("Failed to get player: Out of date");
        }
    }
    sqlite3_reset(get_player);
    SDL_UnlockMutex(mutex);
    return has_player;
}

void save_set_block(int cx, int cz, int bx, int by, int bz, block_t block)
{
    if (!handle)
    {
        return;
    }
    SDL_LockMutex(mutex);
    sqlite3_bind_int(set_block, 1, cx);
    sqlite3_bind_int(set_block, 2, cz);
    sqlite3_bind_int(set_block, 3, bx);
    sqlite3_bind_int(set_block, 4, by);
    sqlite3_bind_int(set_block, 5, bz);
    sqlite3_bind_int(set_block, 6, block);
    if (sqlite3_step(set_block) != SQLITE_DONE)
    {
        SDL_Log("Failed to set block: %s", sqlite3_errmsg(handle));
    }
    sqlite3_reset(set_block);
    SDL_UnlockMutex(mutex);
}

void save_get_blocks(void* userdata, int cx, int cz, save_get_blocks_cb_t cb)
{
    if (!handle)
    {
        return;
    }
    SDL_LockMutex(mutex);
    sqlite3_bind_int(get_blocks, 1, cx);
    sqlite3_bind_int(get_blocks, 2, cz);
    while (sqlite3_step(get_blocks) == SQLITE_ROW)
    {
        int bx = sqlite3_column_int(get_blocks, 0);
        int by = sqlite3_column_int(get_blocks, 1);
        int bz = sqlite3_column_int(get_blocks, 2);
        block_t block = sqlite3_column_int(get_blocks, 3);
        cb(userdata, bx, by, bz, block);
    }
    sqlite3_reset(get_blocks);
    SDL_UnlockMutex(mutex);
}
