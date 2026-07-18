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
static const char* SKY_TABLE =
    "CREATE TABLE IF NOT EXISTS sky ("
    "    id INTEGER PRIMARY KEY NOT NULL,"
    "    time_of_day REAL NOT NULL"
    ");";
static const char* SET_PLAYER = "INSERT OR REPLACE INTO players (id, data) VALUES (0, ?);";
static const char* GET_PLAYER = "SELECT data FROM players WHERE id = 0;";
static const char* SET_SKY = "INSERT OR REPLACE INTO sky (id, time_of_day) VALUES (0, ?);";
static const char* GET_SKY = "SELECT time_of_day FROM sky WHERE id = 0;";
static const char* SET_BLOCK = "INSERT OR REPLACE INTO blocks (cx, cz, bx, by, bz, block) VALUES (?, ?, ?, ?, ?, ?);";
static const char* GET_BLOCKS = "SELECT bx, by, bz, block FROM blocks WHERE cx = ? AND cz = ?;";
static const char* BLOCK_INDEX = "CREATE INDEX IF NOT EXISTS bindex ON blocks (cx, cz);";

static sqlite3* handle;
static sqlite3_stmt* set_player;
static sqlite3_stmt* get_player;
static sqlite3_stmt* set_sky;
static sqlite3_stmt* get_sky;
static sqlite3_stmt* set_block;
static sqlite3_stmt* get_blocks;
static SDL_Mutex* mutex;

static bool Execute(const char* sql, const char* name)
{
    if (sqlite3_exec(handle, sql, NULL, NULL, NULL))
    {
        SDL_Log("Failed to %s: %s", name, sqlite3_errmsg(handle));
        return false;
    }
    return true;
}

static bool Prepare(sqlite3_stmt** statement, const char* sql, const char* name)
{
    if (sqlite3_prepare_v2(handle, sql, -1, statement, NULL))
    {
        SDL_Log("Failed to prepare %s: %s", name, sqlite3_errmsg(handle));
        return false;
    }
    return true;
}

bool Save_Init(const char* path)
{
    if (sqlite3_open(path, &handle))
    {
        SDL_Log("Failed to open %s database: %s", path, sqlite3_errmsg(handle));
        sqlite3_close(handle);
        handle = NULL;
        return false;
    }
    mutex = SDL_CreateMutex();
    if (!mutex)
    {
        SDL_Log("Failed to create mutex: %s", SDL_GetError());
        sqlite3_close(handle);
        handle = NULL;
        return false;
    }
    if (!Execute(PLAYER_TABLE, "create player table") ||
        !Execute(BLOCK_TABLE, "create block table") ||
        !Execute(SKY_TABLE, "create sky table") ||
        !Execute(BLOCK_INDEX, "create block index") ||
        !Prepare(&set_player, SET_PLAYER, "set player") ||
        !Prepare(&get_player, GET_PLAYER, "get player") ||
        !Prepare(&set_sky, SET_SKY, "set sky") ||
        !Prepare(&get_sky, GET_SKY, "get sky") ||
        !Prepare(&set_block, SET_BLOCK, "set block") ||
        !Prepare(&get_blocks, GET_BLOCKS, "get blocks"))
    {
        Save_Free();
        return false;
    }
    sqlite3_exec(handle, "BEGIN;", NULL, NULL, NULL);
    return true;
}

void Save_Free()
{
    if (!handle)
    {
        return;
    }
    SDL_DestroyMutex(mutex);
    sqlite3_exec(handle, "COMMIT;", NULL, NULL, NULL);
    sqlite3_finalize(set_player);
    sqlite3_finalize(get_player);
    sqlite3_finalize(set_sky);
    sqlite3_finalize(get_sky);
    sqlite3_finalize(set_block);
    sqlite3_finalize(get_blocks);
    sqlite3_close(handle);
    handle = NULL;
    set_player = NULL;
    get_player = NULL;
    set_sky = NULL;
    get_sky = NULL;
    set_block = NULL;
    get_blocks = NULL;
    mutex = NULL;
}

void Save_SetPlayer(const void* data, int size)
{
    if (!handle)
    {
        return;
    }
    SDL_LockMutex(mutex);
    sqlite3_bind_blob(set_player, 1, data, size, SQLITE_TRANSIENT);
    if (sqlite3_step(set_player) != SQLITE_DONE)
    {
        SDL_Log("Failed to set player: %s", sqlite3_errmsg(handle));
    }
    sqlite3_reset(set_player);
    SDL_UnlockMutex(mutex);
}

bool Save_GetPlayer(void* data, int size)
{
    if (!handle)
    {
        return false;
    }
    SDL_LockMutex(mutex);
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
            has_player = false;
        }
    }
    sqlite3_reset(get_player);
    SDL_UnlockMutex(mutex);
    return has_player;
}

void Save_SetSky(float time_of_day)
{
    if (!handle)
    {
        return;
    }
    SDL_LockMutex(mutex);
    sqlite3_bind_double(set_sky, 1, time_of_day);
    if (sqlite3_step(set_sky) != SQLITE_DONE)
    {
        SDL_Log("Failed to set sky: %s", sqlite3_errmsg(handle));
    }
    sqlite3_reset(set_sky);
    SDL_UnlockMutex(mutex);
}

bool Save_GetSky(float* time_of_day)
{
    if (!handle)
    {
        return false;
    }
    SDL_LockMutex(mutex);
    bool has_sky = sqlite3_step(get_sky) == SQLITE_ROW;
    if (has_sky)
    {
        *time_of_day = (float) sqlite3_column_double(get_sky, 0);
    }
    sqlite3_reset(get_sky);
    SDL_UnlockMutex(mutex);
    return has_sky;
}

void Save_SetBlock(int cx, int cz, int bx, int by, int bz, Block block)
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

void Save_GetBlocks(void* userdata, int cx, int cz, SaveSetBlock function)
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
        Block block = sqlite3_column_int(get_blocks, 3);
        function(userdata, bx, by, bz, block);
    }
    sqlite3_reset(get_blocks);
    SDL_UnlockMutex(mutex);
}
