#include <SDL3/SDL.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <threads.h>
#include "block.h"
#include "containers.h"
#include "database.h"
#include "helpers.h"

typedef enum
{
    JOB_TYPE_QUIT,
    JOB_TYPE_COMMIT,
    JOB_TYPE_PLAYER,
    JOB_TYPE_BLOCK,
}
job_type_t;

typedef struct
{
    job_type_t type;
    union
    {
        struct
        {
            int player_id;
            float player_x;
            float player_y;
            float player_z;
            float player_pitch;
            float player_yaw;
        };
        struct
        {
            int block_a;
            int block_c;
            int block_x;
            int block_y;
            int block_z;
            block_t block_id;
        };
    };
}
job_t;

static thrd_t thrd;
static mtx_t mtx;
static cnd_t cnd;
static queue_t queue;
static sqlite3* handle;
static sqlite3_stmt* set_player_stmt;
static sqlite3_stmt* get_player_stmt;
static sqlite3_stmt* set_block_stmt;
static sqlite3_stmt* get_blocks_stmt;

static int loop(void* args)
{
    (void) args;
    sqlite3_exec(handle, "BEGIN;", NULL, NULL, NULL);
    while (true)
    {
        job_t job;
        mtx_lock(&mtx);
        while (!queue_remove(&queue, &job))
        {
            cnd_wait(&cnd, &mtx);
        }
        mtx_unlock(&mtx);
        switch (job.type)
        {
        case JOB_TYPE_QUIT:
            sqlite3_exec(handle, "COMMIT;", NULL, NULL, NULL);
            return 0;
        case JOB_TYPE_COMMIT:
            sqlite3_exec(handle, "COMMIT; BEGIN;", NULL, NULL, NULL);
            break;
        case JOB_TYPE_PLAYER:
            sqlite3_bind_int(set_player_stmt, 1, job.player_id);
            sqlite3_bind_double(set_player_stmt, 2, job.player_x);
            sqlite3_bind_double(set_player_stmt, 3, job.player_y);
            sqlite3_bind_double(set_player_stmt, 4, job.player_z);
            sqlite3_bind_double(set_player_stmt, 5, job.player_pitch);
            sqlite3_bind_double(set_player_stmt, 6, job.player_yaw);
            if (sqlite3_step(set_player_stmt) != SQLITE_DONE)
            {
                SDL_Log("Failed to set player: %s", sqlite3_errmsg(handle));
            }
            sqlite3_reset(set_player_stmt);
            break;
        case JOB_TYPE_BLOCK:
            sqlite3_bind_int(set_block_stmt, 1, job.block_a);
            sqlite3_bind_int(set_block_stmt, 2, job.block_c);
            sqlite3_bind_int(set_block_stmt, 3, job.block_x);
            sqlite3_bind_int(set_block_stmt, 4, job.block_y);
            sqlite3_bind_int(set_block_stmt, 5, job.block_z);
            sqlite3_bind_int(set_block_stmt, 6, job.block_id);
            if (sqlite3_step(set_block_stmt) != SQLITE_DONE)
            {
                SDL_Log("Failed to set block: %s", sqlite3_errmsg(handle));
            }
            sqlite3_reset(set_block_stmt);
            break;
        }
    }
    return 0;
}

bool database_init(const char* file)
{
    assert(file);
    const char* players_table =
        "CREATE TABLE IF NOT EXISTS players ("
        "    id INT PRIMARY KEY NOT NULL,"
        "    x REAL NOT NULL,"
        "    y REAL NOT NULL,"
        "    z REAL NOT NULL,"
        "    pitch REAL NOT NULL,"
        "    yaw REAL NOT NULL"
        ");";
    const char* blocks_table =
        "CREATE TABLE IF NOT EXISTS blocks ("
        "    a INTEGER NOT NULL,"
        "    c INTEGER NOT NULL,"
        "    x INTEGER NOT NULL,"
        "    y INTEGER NOT NULL,"
        "    z INTEGER NOT NULL,"
        "    data INTEGER NOT NULL"
        ");";
    const char* blocks_index =
        "CREATE INDEX IF NOT EXISTS blocks_index "
        "ON blocks (a, c);";
    const char* set_player =
        "INSERT OR REPLACE INTO players (id, x, y, z, pitch, yaw) "
        "VALUES (?, ?, ?, ?, ?, ?);";
    const char* get_player =
        "SELECT x, y, z, pitch, yaw FROM players "
        "WHERE id = ?;";
    const char* set_block =
        "INSERT OR REPLACE INTO blocks (a, c, x, y, z, data) "
        "VALUES (?, ?, ?, ?, ?, ?);";
    const char* get_blocks =
        "SELECT x, y, z, data FROM blocks "
        "WHERE a = ? AND c = ?;";
    queue_init(&queue, DATABASE_MAX_JOBS, sizeof(job_t));
    if (sqlite3_open(file, &handle))
    {
        SDL_Log("Failed to open %s database: %s", file, sqlite3_errmsg(handle));
        return false;
    }
    if (sqlite3_exec(handle, players_table, NULL, NULL, NULL))
    {
        SDL_Log("Failed to create players table: %s", sqlite3_errmsg(handle));
        return false;
    }
    if (sqlite3_exec(handle, blocks_table, NULL, NULL, NULL))
    {
        SDL_Log("Failed to create blocks table: %s", sqlite3_errmsg(handle));
        return false;
    }
    if (sqlite3_exec(handle, blocks_index, NULL, NULL, NULL))
    {
        SDL_Log("Failed to create blocks index: %s", sqlite3_errmsg(handle));
        return false;
    }
    if (sqlite3_prepare_v2(handle, set_player, -1, &set_player_stmt, NULL))
    {
        SDL_Log("Failed to prepare set player: %s", sqlite3_errmsg(handle));
        return false;
    }
    if (sqlite3_prepare_v2(handle, get_player, -1, &get_player_stmt, NULL))
    {
        SDL_Log("Failed to prepare get player: %s", sqlite3_errmsg(handle));
        return false;
    }
    if (sqlite3_prepare_v2(handle, set_block, -1, &set_block_stmt, NULL))
    {
        SDL_Log("Failed to prepare set block: %s", sqlite3_errmsg(handle));
        return false;
    }
    if (sqlite3_prepare_v2(handle, get_blocks, -1, &get_blocks_stmt, NULL))
    {
        SDL_Log("Failed to prepare get blocks: %s", sqlite3_errmsg(handle));
        return false;
    }
    if (mtx_init(&mtx, mtx_plain) != thrd_success)
    {
        SDL_Log("Failed to create mutex");
        return false;
    }
    if (cnd_init(&cnd) != thrd_success)
    {
        SDL_Log("Failed to create condition variable");
        return false;
    }
    if (thrd_create(&thrd, loop, NULL) != thrd_success)
    {
        SDL_Log("Failed to create thread");
        return false;
    }
    return true;
}

void database_free()
{
    job_t job;
    job.type = JOB_TYPE_QUIT;
    mtx_lock(&mtx);
    while (!queue_add(&queue, &job, false))
    {
        SDL_Log("Failed to add quit job");
    }
    cnd_signal(&cnd);
    mtx_unlock(&mtx);
    thrd_join(thrd, NULL);
    mtx_destroy(&mtx);
    cnd_destroy(&cnd);
    queue_free(&queue);
    sqlite3_finalize(set_player_stmt);
    sqlite3_finalize(get_player_stmt);
    sqlite3_finalize(set_block_stmt);
    sqlite3_close(handle);
    handle = NULL;
}

void database_commit()
{
    job_t job;
    job.type = JOB_TYPE_COMMIT;
    mtx_lock(&mtx);
    if (!queue_add(&queue, &job, false))
    {
        SDL_Log("Failed to add commit job");
    }
    cnd_signal(&cnd);
    mtx_unlock(&mtx);
}

void database_set_player(
    const int id,
    const float x,
    const float y,
    const float z,
    const float pitch,
    const float yaw)
{
    job_t job;
    job.type = JOB_TYPE_PLAYER;
    job.player_id = id;
    job.player_x = x;
    job.player_y = y;
    job.player_z = z;
    job.player_pitch = pitch;
    job.player_yaw = yaw;
    mtx_lock(&mtx);
    if (!queue_add(&queue, &job, false))
    {
        SDL_Log("Failed to add player job");
    }
    cnd_signal(&cnd);
    mtx_unlock(&mtx);
}

bool database_get_player(
    const int id,
    float* x,
    float* y,
    float* z,
    float* pitch,
    float* yaw)
{
    assert(x);
    assert(y);
    assert(z);
    assert(pitch);
    assert(yaw);
    mtx_lock(&mtx);
    sqlite3_bind_int(get_player_stmt, 1, id);
    bool player = sqlite3_step(get_player_stmt) == SQLITE_ROW;
    if (player)
    {
        *x = sqlite3_column_double(get_player_stmt, 0);
        *y = sqlite3_column_double(get_player_stmt, 1);
        *z = sqlite3_column_double(get_player_stmt, 2);
        *pitch = sqlite3_column_double(get_player_stmt, 3);
        *yaw = sqlite3_column_double(get_player_stmt, 4);
    }
    sqlite3_reset(get_player_stmt);
    mtx_unlock(&mtx);
    return player;
}

void database_set_block(
    const int a,
    const int c,
    const int x,
    const int y,
    const int z,
    const block_t block)
{
    job_t job;
    job.type = JOB_TYPE_BLOCK;
    job.block_a = a;
    job.block_c = c;
    job.block_x = x;
    job.block_y = y;
    job.block_z = z;
    job.block_id = block;
    mtx_lock(&mtx);
    if (!queue_add(&queue, &job, false))
    {
        SDL_Log("Failed to add block job");
    }
    cnd_signal(&cnd);
    mtx_unlock(&mtx);
}

void database_get_blocks(
    group_t* group,
    const int a,
    const int c)
{
    assert(group);
    mtx_lock(&mtx);
    sqlite3_bind_int(get_blocks_stmt, 1, a);
    sqlite3_bind_int(get_blocks_stmt, 2, c);
    while (sqlite3_step(get_blocks_stmt) == SQLITE_ROW)
    {
        const int x = sqlite3_column_int(get_blocks_stmt, 0);
        const int y = sqlite3_column_int(get_blocks_stmt, 1);
        const int z = sqlite3_column_int(get_blocks_stmt, 2);
        const block_t block = sqlite3_column_int(get_blocks_stmt, 3);
        group_set_block(group, x, y, z, block);
    }
    sqlite3_reset(get_blocks_stmt);
    mtx_unlock(&mtx);
}