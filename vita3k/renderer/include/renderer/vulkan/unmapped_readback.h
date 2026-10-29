// Vita3K Uncharted iOS integration, GPL-2.0-or-later.
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace renderer::vulkan {
inline uint64_t readback_footprint(uint32_t width, uint32_t height, uint32_t stride,
    uint32_t bytes, bool tiled) {
    return tiled ? ((uint64_t(width) + 31) / 32) * ((uint64_t(height) + 31) / 32) * 1024 * bytes
                 : uint64_t(stride) * height * bytes;
}

// The GPU copy already writes guest-order 32x32 tiles for tiled surfaces.
// Copy only rendered texels: never overwrite CPU-owned padding or atlas regions.
inline bool copy_unmapped_readback(uint8_t *dst, uint64_t dst_size,
    const uint8_t *src, uint64_t src_size, uint32_t surface_width,
    uint32_t surface_height, uint32_t stride, uint32_t bytes, bool tiled,
    uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    if (!dst || !src || !bytes || x > surface_width || y > surface_height
        || width > surface_width - x || height > surface_height - y
        || surface_width > stride)
        return false;
    const uint64_t footprint = readback_footprint(surface_width, surface_height, stride, bytes, tiled);
    if (footprint > dst_size || footprint > src_size)
        return false;
    for (uint32_t row = y; row < y + height; ++row) {
        for (uint32_t column = x; column < x + width;) {
            const uint32_t count = tiled ? std::min(x + width - column, 32 - column % 32) : width;
            const uint64_t pixel = tiled
                ? ((uint64_t(row / 32) * ((uint64_t(surface_width) + 31) / 32) + column / 32) * 1024 + (row % 32) * 32 + column % 32)
                : uint64_t(row) * stride + column;
            std::memcpy(dst + pixel * bytes, src + pixel * bytes, size_t(count) * bytes);
            column += count;
        }
    }
    return true;
}
}
