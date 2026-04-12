#pragma once
/**
 * Shared background worker task.
 *
 * Rationale: previously each long-running operation (saveRoutes, savePeers,
 * reportTopology) spawned its own FreeRTOS task via xTaskCreate. Each task
 * carved a 4 KB stack out of the heap, and when multiple ran concurrently
 * (typical during PERSIST_INTERVAL bursts), the heap was permanently split
 * into smaller blocks — `maxBlock` dropped and never recovered, eventually
 * wedging AsyncTCP/lwIP which needs >32 KB contiguous buffers.
 *
 * The bgWorker allocates its stack ONCE at boot (when free heap is still
 * ~200 KB and contiguous), and all background work is pushed onto a queue
 * and executed serially by this single task. No more task-stack
 * fragmentation.
 */

#include <Arduino.h>

typedef void (*BgWorkerFn)(void);

/** Initialise the background worker. Call once during setup(). */
void bgWorkerInit();

/**
 * Enqueue a function to be executed by the worker.
 * Returns false if the queue is full or the worker is not initialised.
 * The function runs to completion on the worker task (stack ~8 KB).
 */
bool bgWorkerEnqueue(BgWorkerFn fn);

/** Number of items currently pending in the queue. */
size_t bgWorkerPending();
