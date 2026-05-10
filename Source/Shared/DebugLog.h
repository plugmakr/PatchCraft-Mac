#pragma once
#include <cstdio>
#include <cstdlib>
#include <ctime>

inline FILE* pcLogFile()
{
    static FILE* f = nullptr;
    if (! f)
    {
        const char* tmp = std::getenv("TEMP");
        if (!tmp) tmp = std::getenv("TMP");
        if (!tmp) tmp = ".";
        char path[512];
        snprintf(path, sizeof(path), "%s\\pcraft_debug.log", tmp);
        f = fopen(path, "a");
        if (f) { fprintf(f, "\n=== NEW SESSION ===\n"); fflush(f); }
    }
    return f;
}

#define PC_DBG(fmt, ...) do { \
    if (auto* _f = pcLogFile()) { \
        fprintf(_f, "[PC] " fmt "\n", ##__VA_ARGS__); \
        fflush(_f); \
    } \
} while(0)
