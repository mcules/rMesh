#include "statusDisplay.h"

// Weak no-op fallbacks. Display drivers that own a screen capable of
// rendering a splash/flashing notification override these with strong symbols
// in their own translation unit; on boards without such a driver the linker
// keeps these defaults so that platform-agnostic call sites still link.
__attribute__((weak)) void showStatusDisplaySplash(uint32_t /*holdMs*/) {}
__attribute__((weak)) void showStatusDisplayFlashing(const char* /*what*/) {}