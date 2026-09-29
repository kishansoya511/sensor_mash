#pragma once

#include <stddef.h>
#include <stdint.h>

// Provided recursive implementation (algorithm logic unchanged).
static inline uint32_t crc32_recursive(const uint8_t* data,
                                      size_t len,
                                      uint32_t crc)
{
    if (len == 0u) return crc ^ 0xFFFFFFFFu;

    uint32_t byte_crc = crc ^ (uint32_t)(*data);
    for (int i = 0; i < 8; ++i)
        byte_crc = (byte_crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int)(byte_crc & 1u)));

    return crc32_recursive(data + 1u, len - 1u, byte_crc);
}

// Iterative form with identical per-byte update logic (avoids deep call stacks).
static inline uint32_t crc32_iterative(const uint8_t* data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        uint32_t byte_crc = crc ^ (uint32_t)data[i];
        for (int b = 0; b < 8; ++b)
            byte_crc = (byte_crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int)(byte_crc & 1u)));
        crc = byte_crc;
    }
    return crc ^ 0xFFFFFFFFu;
}


