#include "util/bgWorker.h"
#include "util/logging.h"
#include "util/heapdbg.h"

#ifdef ESP32
  #include <freertos/FreeRTOS.h>
  #include <freertos/task.h>
  #include <freertos/queue.h>
#else
  #include <FreeRTOS.h>
  #include <task.h>
  #include <queue.h>
#endif

#define BG_QUEUE_SIZE   8
#define BG_STACK_BYTES  8192

static QueueHandle_t s_queue = nullptr;
static TaskHandle_t  s_task  = nullptr;

static void bgWorkerTask(void* /*pv*/) {
    BgWorkerFn fn = nullptr;
    for (;;) {
        if (xQueueReceive(s_queue, &fn, portMAX_DELAY) == pdTRUE && fn != nullptr) {
            // Execute the work item serially. Any heap churn this causes
            // is contained to this single persistent task — its stack is
            // never reallocated, so no fragmentation from task creation.
            fn();
        }
    }
}

void bgWorkerInit() {
    if (s_queue != nullptr) return;
    s_queue = xQueueCreate(BG_QUEUE_SIZE, sizeof(BgWorkerFn));
    if (!s_queue) {
        logPrintf(LOG_ERROR, "BgWorker", "queue create failed");
        return;
    }
    BaseType_t ok = xTaskCreate(bgWorkerTask, "bgWorker",
                                BG_STACK_BYTES / sizeof(StackType_t),
                                nullptr, 1, &s_task);
    if (ok != pdPASS) {
        logPrintf(LOG_ERROR, "BgWorker", "task create failed");
        vQueueDelete(s_queue);
        s_queue = nullptr;
        return;
    }
    HEAP_MARK("bgWorker/init");
    logPrintf(LOG_INFO, "BgWorker", "initialised (queue=%d, stack=%d)",
              BG_QUEUE_SIZE, BG_STACK_BYTES);
}

bool bgWorkerEnqueue(BgWorkerFn fn) {
    if (!s_queue || !fn) return false;
    return xQueueSend(s_queue, &fn, 0) == pdTRUE;
}

size_t bgWorkerPending() {
    if (!s_queue) return 0;
    return (size_t)uxQueueMessagesWaiting(s_queue);
}
