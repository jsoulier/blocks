#include <SDL3/SDL.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <threads.h>
#include "helpers.h"
#include "database.h"
#include "containers.h"

#define MAX_JOBS 100

typedef enum {
    JOB_TYPE_QUIT,
    JOB_TYPE_PLAYER,
    JOB_TYPE_BLOCK,
    JOB_TYPE_COMMIT,
} job_type_t;

typedef struct {
    int id;
    float x, y, z;
    float pitch, yaw;
} job_player_t;

typedef struct {
    job_type_t type;
    union {
        job_player_t player;
    };
} job_t;

static ring_t jobs;
static thrd_t thrd;
static mtx_t mtx;
static cnd_t cnd;
static sqlite3* handle;
static sqlite3_stmt* set_player_stmt;
static sqlite3_stmt* get_player_stmt;

static void set_player(const job_t* job)
{
    sqlite3_bind_int(set_player_stmt, 1, job->player.id);
    sqlite3_bind_double(set_player_stmt, 2, job->player.x);
    sqlite3_bind_double(set_player_stmt, 3, job->player.y);
    sqlite3_bind_double(set_player_stmt, 4, job->player.z);
    sqlite3_bind_double(set_player_stmt, 5, job->player.pitch);
    sqlite3_bind_double(set_player_stmt, 6, job->player.yaw);
    if (sqlite3_step(set_player_stmt) != SQLITE_DONE) {
        SDL_Log("Failed to set player %d: %s", job->player.id, sqlite3_errmsg(handle));
    }
    sqlite3_reset(set_player_stmt);
}

static int loop(void* args)
{
    sqlite3_exec(handle, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    while (true) {
        job_t job;
        mtx_lock(&mtx);
        while (!ring_remove(&jobs, &job)) {
            cnd_wait(&cnd, &mtx);
        }
        mtx_unlock(&mtx);
        switch (job.type) {
        case JOB_TYPE_QUIT:
            sqlite3_exec(handle, "COMMIT;", NULL, NULL, NULL);
            return 0;
        case JOB_TYPE_PLAYER:
            set_player(&job);
            break;
        case JOB_TYPE_BLOCK:
            break;
        case JOB_TYPE_COMMIT:
            sqlite3_exec(handle, "COMMIT; BEGIN TRANSACTION;", NULL, NULL, NULL);
            break;
        }
    }
    return 0;
}

static void dispatch(const job_t* job)
{
    mtx_lock(&mtx);
    if (!ring_add(&jobs, job, false)) {
        SDL_Log("Failed to add job");
    }
    cnd_signal(&cnd);
    mtx_unlock(&mtx);
}

bool database_init(const char* file)
{
    assert(file);
    ring_init(&jobs, MAX_JOBS, sizeof(job_t));
    if (sqlite3_open(file, &handle) != SQLITE_OK) {
        SDL_Log("Failed to open %s database: %s", file, sqlite3_errmsg(handle));
        return false;
    }
    const char* player_table =
        "CREATE TABLE IF NOT EXISTS players ("
        "id INT PRIMARY KEY NOT NULL, "
        "x REAL NOT NULL, "
        "y REAL NOT NULL, "
        "z REAL NOT NULL, "
        "pitch REAL NOT NULL, "
        "yaw REAL NOT NULL"
        ");";
    const char* block_table =
        "CREATE TABLE IF NOT EXISTS blocks ("
        "a INTEGER NOT NULL, "
        "c INTEGER NOT NULL, "
        "x INTEGER NOT NULL, "
        "y INTEGER NOT NULL, "
        "z INTEGER NOT NULL, "
        "data INTEGER NOT NULL "
        ");";
    const char* set_player =
        "INSERT OR REPLACE INTO players (id, x, y, z, pitch, yaw) "
        "VALUES (?, ?, ?, ?, ?, ?);";
    const char* get_player =
        "SELECT x, y, z, pitch, yaw FROM players "
        "WHERE id = ?;";
    if (sqlite3_exec(handle, player_table, NULL, NULL, NULL) != SQLITE_OK) {
        SDL_Log("Failed to create players table: %s", sqlite3_errmsg(handle));
        return false;
    }
    if (sqlite3_exec(handle, block_table, NULL, NULL, NULL) != SQLITE_OK) {
        SDL_Log("Failed to create blocks table: %s", sqlite3_errmsg(handle));
        return false;
    }
    if (sqlite3_prepare_v2(handle, set_player, -1, &set_player_stmt, NULL) != SQLITE_OK) {
        SDL_Log("Failed to prepare set player statement: %s", sqlite3_errmsg(handle));
        return false;
    }
    if (sqlite3_prepare_v2(handle, get_player, -1, &get_player_stmt, NULL) != SQLITE_OK) {
        SDL_Log("Failed to prepare get player statement: %s", sqlite3_errmsg(handle));
        return false;
    }
    if (mtx_init(&mtx, mtx_plain) != thrd_success) {
        SDL_Log("Failed to create mutex");
        return false;
    }
    if (cnd_init(&cnd) != thrd_success) {
        SDL_Log("Failed to create condition variable");
        return false;
    }
    if (thrd_create(&thrd, loop, NULL) != thrd_success) {
        SDL_Log("Failed to create thread");
        return false;
    }
    return true;
}

void database_free()
{
    job_t job;
    job.type = JOB_TYPE_QUIT;
    dispatch(&job);
    thrd_join(thrd, NULL);
    mtx_destroy(&mtx);
    cnd_destroy(&cnd);
    sqlite3_finalize(set_player_stmt);
    sqlite3_finalize(get_player_stmt);
    sqlite3_close(handle);
    ring_free(&jobs);
    handle = NULL;
}

void database_commit()
{
    job_t job;
    job.type = JOB_TYPE_COMMIT;
    dispatch(&job);
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
    job.player.id = id;
    job.player.x = x;
    job.player.y = y;
    job.player.z = z;
    job.player.pitch = pitch;
    job.player.yaw = yaw;
    dispatch(&job);
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
    sqlite3_bind_int(get_player_stmt, 1, id);
    if (sqlite3_step(get_player_stmt) == SQLITE_ROW) {
        *x = sqlite3_column_double(get_player_stmt, 0);
        *y = sqlite3_column_double(get_player_stmt, 1);
        *z = sqlite3_column_double(get_player_stmt, 2);
        *pitch = sqlite3_column_double(get_player_stmt, 3);
        *yaw = sqlite3_column_double(get_player_stmt, 4);
        sqlite3_reset(get_player_stmt);
        return true;
    } else {
        SDL_Log("Failed to get player %d: %s", id, sqlite3_errmsg(handle));
        sqlite3_reset(get_player_stmt);
        return false;
    }
}