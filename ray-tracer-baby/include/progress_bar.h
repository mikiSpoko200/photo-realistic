#pragma once

#include <cmm/cmm.h>
#include <stdatomic.h>
#include <time.h>


#define atomic _Atomic

enum TaskDisplayFormat {
    TDFPercentage = 0,
};

typedef struct {
    usize progress;
    usize end;
    usize id;
} STask;

DeclareArray(STask);

#ifndef X_PRINT
#define X_PRINT(fmt, ...) PRINTLN(fmt, __VA_ARGS__)
#endif


#if true // (__STDC_VERSION__ >= 201112L) && (__STDC_NO_ATOMICS__ != 1)
typedef struct {
    atomic usize progress;
    atomic usize end;
    usize id;
} PTask;

DeclareArray(PTask);

typedef struct {
    Array(STask) sTasks;
    Array(PTask) pTasks;
} Tasks;

typedef struct {
    Tasks tasks;
    time_t start;
} Display;

internal void SetupDisplay(const Display* const display) {
    const usize nTasks = display->tasks.sTasks.len + display->tasks.pTasks.len;
    for (usize i = 0; i < nTasks; i++) X_PRINT("%5d%%", 0.f);
}

internal void UpdateDisplay(volatile Display* const display) {
    // TODO: move these ASCII escapes into macros
    const usize nSTasks = display->tasks.sTasks.len;
    const usize nPTasks = display->tasks.pTasks.len;
    const usize nTasks = nPTasks + nSTasks;
    printf("\033[%luF", nTasks);
    for (usize i = 0; i < nSTasks; i++) {
        // TODO: czy tutaj muszę dawać dwa razy f32
        const f32 completed = (f32)display->tasks.sTasks.data[i].progress / (f32)display->tasks.sTasks.data[i].end;
        const f32 remaining = 1.f - completed;
        const time_t elapsed = difftime(time(NULL), display->start);
        const f32 estimation = elapsed / completed;
        X_PRINT(
            "%05lu%% | estim. %3lu s left", 
            (usize)(completed * 100),
            (usize)(estimation * remaining)
        );
    }
    for (usize i = 0; i < nPTasks; i++) {
        const f32 completed = (f32)display->tasks.pTasks.data[i].progress / (f32)display->tasks.pTasks.data[i].end;
        const f32 remaining = 1.f - completed;
        const time_t elapsed = difftime(time(NULL), display->start);
        const f32 estimation = (f32)elapsed / completed;
        X_PRINT(
            "%5lu%% | estim. %3lu s left", 
            (usize)(completed * 100),
            (usize)(estimation * remaining)
        );
    }
}

internal bool FinishedDisplay(Display* const display) {
    const usize nSTasks = display->tasks.sTasks.len;
    const usize nPTasks = display->tasks.pTasks.len;
    bool done = true;
    for (usize i = 0; i < nSTasks; i++) {
        done &= display->tasks.sTasks.data[i].progress == display->tasks.sTasks.data[i].end;
    }
    for (usize i = 0; i < nPTasks; i++) {
        done &= display->tasks.pTasks.data[i].progress == display->tasks.pTasks.data[i].end;
    }
    return done;
}
#else

typedef struct {
    Array(STask) sTasks;
} Tasks;

typedef struct {
    Tasks tasks;
} Display;

internal void SetupDisplay(const Display* const display) {
    for (usize i = 0; i < display->tasks.sTasks.len; i++) X_PRINT("%.2f%%", 0.f);
}

internal void InitializeDisplay(Display* display, Tasks tasks) {
    display->tasks = tasks;
    SetupDisplay(display);
}

internal void UpdateDisplay(Display* const display) {
    // TODO: move these ASCII escapes into macros
    const usize nSTasks;
    printf("\033[%dF", );
    for (usize i = 0; i < display->tasks.sTasks.len; i++) {
        X_PRINT("%.2f%%", ((f32)display->tasks.sTasks.data[i].progress * 100.f) / (f32)display->tasks.sTasks.data[i].end); 
    }
}
#endif
#undef X_PRINT
