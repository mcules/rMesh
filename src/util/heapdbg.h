#pragma once
/**
 * Lightweight heap instrumentation for tracking down fragmentation / leaks.
 *
 *   HEAP_MARK("tag")        -> logs one-shot snapshot + records event
 *   HEAP_SCOPE("tag")       -> RAII: logs delta after, records event
 *   heapTick()              -> call from loop; periodic snapshot every 30s
 *
 * All events are also kept in a small ring buffer that api.cpp exposes
 * via /api/status diagnostics.heapLog so the bridgeserver can see them.
 */

#include <Arduino.h>
#include "util/logging.h"

// Runtime toggle for heap instrumentation. Default OFF — when disabled,
// all HEAP_MARK/HEAP_SCOPE/heapTick calls become no-ops so the normal
// operating load is unchanged. Enable via the "heapDebug" setting.
extern bool heapDebugEnabled;

#ifdef ESP32
#include <esp_heap_caps.h>

struct HeapEvent {
    uint32_t uptime;       // millis() at event time
    int32_t  freeDelta;    // post-pre (0 for MARK)
    int32_t  maxBlockDelta;
    uint32_t freeAfter;
    uint32_t maxBlockAfter;
    char     tag[24];
};

void heapMark(const char* tag);
void heapRecord(const char* tag, uint32_t freeBefore, uint32_t maxBlockBefore);
void heapTick();

// Number of entries currently in the ring buffer (up to HEAPDBG_RING_SIZE).
size_t heapDbgCount();
// Access entries in chronological order: idx 0 = oldest.
const HeapEvent& heapDbgAt(size_t idx);

#define HEAPDBG_RING_SIZE 24

struct HeapScope {
    const char* tag;
    uint32_t    freeBefore;
    uint32_t    maxBlockBefore;
    HeapScope(const char* t)
        : tag(t),
          freeBefore(ESP.getFreeHeap()),
          maxBlockBefore(ESP.getMaxAllocHeap()) {}
    ~HeapScope() { heapRecord(tag, freeBefore, maxBlockBefore); }
};

#define HEAP_MARK(tag)  heapMark(tag)
#define HEAP_SCOPE_CAT2(a,b) a##b
#define HEAP_SCOPE_CAT(a,b) HEAP_SCOPE_CAT2(a,b)
#define HEAP_SCOPE(tag) HeapScope HEAP_SCOPE_CAT(_heapScope_, __LINE__)(tag)

#else
// nRF52 / other: no-op
inline void heapMark(const char*) {}
inline void heapRecord(const char*, uint32_t, uint32_t) {}
inline void heapTick() {}
inline size_t heapDbgCount() { return 0; }
#define HEAP_MARK(tag)  ((void)0)
#define HEAP_SCOPE(tag) ((void)0)
#endif
