#pragma once
// Native unit-test stub — shadows the real helperFunctions header (which pulls in
// FS/task/Arduino dependencies). frame.cpp only needs safeUtf8Copy from it; the
// test provides a faithful-enough implementation (length clamp + null-terminate).
#include <cstdint>
#include <cstddef>
void safeUtf8Copy(char* dest, const uint8_t* src, size_t srcLen, size_t dstSize);
