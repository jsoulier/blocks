#pragma once

typedef struct sqlite3 sqlite3;

typedef struct save
{
    sqlite3* handle;
}
save_t;
