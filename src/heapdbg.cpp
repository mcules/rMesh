#include "heapdbg.h"

bool heapDebugEnabled = false;

#ifdef ESP32

static HeapEvent s_ring[HEAPDBG_RING_SIZE];
static uint8_t   s_head  = 0;   // next write slot
static uint8_t   s_count = 0;   // how many valid entries

static void ringPush(const char* tag, uint32_t freeBefore, uint32_t maxBlockBefore,
                     uint32_t freeAfter, uint32_t maxBlockAfter) {
    HeapEvent &e = s_ring[s_head];
    e.uptime        = millis();
    e.freeDelta     = (int32_t)freeAfter     - (int32_t)freeBefore;
    e.maxBlockDelta = (int32_t)maxBlockAfter - (int32_t)maxBlockBefore;
    e.freeAfter     = freeAfter;
    e.maxBlockAfter = maxBlockAfter;
    strlcpy(e.tag, tag, sizeof(e.tag));
    s_head = (s_head + 1) % HEAPDBG_RING_SIZE;
    if (s_count < HEAPDBG_RING_SIZE) s_count++;
}

size_t heapDbgCount() { return s_count; }

const HeapEvent& heapDbgAt(size_t idx) {
    // Oldest entry is the one right after head (if ring full), otherwise slot 0.
    size_t base = (s_count < HEAPDBG_RING_SIZE) ? 0 : s_head;
    return s_ring[(base + idx) % HEAPDBG_RING_SIZE];
}

void heapMark(const char* tag) {
    if (!heapDebugEnabled) return;
    uint32_t f  = ESP.getFreeHeap();
    uint32_t mb = ESP.getMaxAllocHeap();
    logPrintf(LOG_INFO, "HEAP", "%s free=%u maxBlock=%u minEver=%u",
              tag, (unsigned)f, (unsigned)mb, (unsigned)ESP.getMinFreeHeap());
    ringPush(tag, f, mb, f, mb);
}

void heapRecord(const char* tag, uint32_t freeBefore, uint32_t maxBlockBefore) {
    if (!heapDebugEnabled) return;
    uint32_t freeAfter     = ESP.getFreeHeap();
    uint32_t maxBlockAfter = ESP.getMaxAllocHeap();
    int32_t  fd = (int32_t)freeAfter     - (int32_t)freeBefore;
    int32_t  md = (int32_t)maxBlockAfter - (int32_t)maxBlockBefore;
    // Only log if something meaningful happened (>256 B) to avoid log spam.
    if (fd <= -256 || fd >= 256 || md <= -256 || md >= 256) {
        logPrintf(LOG_INFO, "HEAP",
                  "%s free=%u (%+ld) maxBlock=%u (%+ld) minEver=%u",
                  tag,
                  (unsigned)freeAfter, (long)fd,
                  (unsigned)maxBlockAfter, (long)md,
                  (unsigned)ESP.getMinFreeHeap());
    }
    ringPush(tag, freeBefore, maxBlockBefore, freeAfter, maxBlockAfter);
}

void heapTick() {
    if (!heapDebugEnabled) return;
    static uint32_t nextTick = 0;
    uint32_t now = millis();
    if ((int32_t)(now - nextTick) < 0) return;
    nextTick = now + 10000;
    heapMark("tick");
}

#endif // ESP32
