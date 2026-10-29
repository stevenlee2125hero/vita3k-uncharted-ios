#include <renderer/vulkan/unmapped_readback.h>
#include <cassert>
#include <vector>
using namespace renderer::vulkan;
int main() {
    for (bool tiled : {false, true})
        for (uint32_t width : {1u, 17u, 32u, 33u, 65u, 720u})
            for (uint32_t height : {1u, 17u, 32u, 33u, 408u})
                for (uint32_t bytes : {1u, 2u, 4u, 8u, 16u}) {
                    const uint32_t stride = width + 7;
                    const auto size = readback_footprint(width, height, stride, bytes, tiled);
                    std::vector<uint8_t> src(size), dst(size + 16, 0xa5), expected(dst);
                    for (size_t i = 0; i < src.size(); ++i)
                        src[i] = uint8_t(i * 17 + i / 251);
                    const uint32_t x0 = width / 3, y0 = height / 3;
                    const uint32_t w = width - x0, h = height - y0;
                    // Reference walks every stored texel in storage order, rather
                    // than deriving the implementation's per-run offsets.
                    for (uint64_t pixel = 0; pixel < size / bytes; ++pixel) {
                        uint64_t x, y;
                        if (tiled) {
                            const auto tiles = (width + 31) / 32;
                            const auto tile = pixel / 1024, local = pixel % 1024;
                            x = tile % tiles * 32 + local % 32;
                            y = tile / tiles * 32 + local / 32;
                        } else {
                            x = pixel % stride;
                            y = pixel / stride;
                        }
                        if (x >= x0 && x < x0 + w && y >= y0 && y < y0 + h)
                            for (uint32_t byte = 0; byte < bytes; ++byte)
                                expected[pixel * bytes + byte] = src[pixel * bytes + byte];
                    }
                    assert(copy_unmapped_readback(dst.data(), size, src.data(), size, width, height, stride, bytes, tiled, x0, y0, w, h));
                    assert(dst == expected);
                    const auto saved = dst;
                    assert(!copy_unmapped_readback(dst.data(), size - 1, src.data(), size, width, height, stride, bytes, tiled, 0, 0, width, height));
                    assert(dst == saved);
                    assert(!copy_unmapped_readback(dst.data(), size, src.data(), size, width, height, stride, bytes, tiled, width, 0, 1, 1));
                    assert(dst == saved);
                }
}
