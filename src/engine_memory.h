// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <cstdint>
#include <cstring>

// Reading and writing single fields of structures the ENGINE laid out.
//
// Every multi-byte access goes through memcpy rather than a typed pointer. The
// mod holds a char* into memory it did not allocate, so nothing here may assume
// the alignment a float* or a void** would promise, and a misaligned typed load
// is undefined behaviour the optimiser is entitled to act on. Single bytes have
// no alignment to get wrong, which is why ReadByte and MarkDirty index directly.
//
// This is the one place either rule lives. Every accessor of an engine-laid-out
// structure goes through this header, so there is one place for the next one to
// be written correctly rather than four.
namespace SomaHT::engine {

inline float ReadFloat(const void* base, uint32_t offset) {
    float value = 0.0f;
    std::memcpy(&value, static_cast<const char*>(base) + offset, sizeof(value));
    return value;
}

inline void WriteFloat(void* base, uint32_t offset, float value) {
    std::memcpy(static_cast<char*>(base) + offset, &value, sizeof(value));
}

inline const char* ReadPointer(const void* base, uint32_t offset) {
    const char* value = nullptr;
    std::memcpy(&value, static_cast<const char*>(base) + offset, sizeof(value));
    return value;
}

inline void ReadVec2(const void* base, uint32_t offset, float out[2]) {
    std::memcpy(out, static_cast<const char*>(base) + offset, sizeof(float) * 2);
}

inline char ReadByte(const void* base, uint32_t offset) {
    return static_cast<const char*>(base)[offset];
}

// The engine's lazy-rebuild flags: one byte per cached matrix, set to make the
// engine rebuild it from the fields the mod just wrote. A write that skips the
// matching byte leaves the render using the matrix the previous tick built.
inline void MarkDirty(void* base, uint32_t offset) {
    static_cast<char*>(base)[offset] = 1;
}

}  // namespace SomaHT::engine
