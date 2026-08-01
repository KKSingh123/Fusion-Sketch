#ifndef TAILOREDSKETCH_CODE_KK_ABLATION_SKETCHES_H
#define TAILOREDSKETCH_CODE_KK_ABLATION_SKETCHES_H

// Ablation baselines for KK_Sketch.  They intentionally keep the hashing,
// counter types, and three-row layout of the corresponding KK component while
// assigning the full supplied memory budget to that component.

#include "MurmurHash.h"
#include "Sketch.h"
#include "params.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace kk_ablation_detail {
constexpr uint32_t kRows = 3;
constexpr uint32_t kMinWidth = 64;

inline uint32_t fast_range(uint32_t hash, uint32_t range) {
    return static_cast<uint32_t>((static_cast<uint64_t>(hash) * range) >> 32);
}

inline uint32_t mix32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

inline uint32_t stride_hash(uint32_t hash) {
    return mix32(hash ^ 0x9e3779b9U) | 1U;
}

inline int residual_sign(uint32_t hash, uint32_t stride, uint32_t row) {
    return (((hash >> (row + 9U)) ^ (stride >> (row + 3U))) & 1U) ? 1 : -1;
}
}  // namespace kk_ablation_detail

class CU_Only_Sketch : public Sketch {
public:
    CU_Only_Sketch(int bytes, int, int hash_seed = 1000)
        : seed_(static_cast<uint32_t>(hash_seed)) {
        width_ = std::max(static_cast<uint32_t>(std::max(bytes, 0)) /
                              (static_cast<uint32_t>(sizeof(uint16_t)) * kk_ablation_detail::kRows),
                          kk_ablation_detail::kMinWidth);
        counters_.assign(width_ * kk_ablation_detail::kRows, 0);
    }

    void Insert(const char* str) override {
        const uint32_t hash = MurmurHash32(str, KEY_LEN, seed_);
        const uint32_t stride = kk_ablation_detail::stride_hash(hash);
        uint32_t indices[kk_ablation_detail::kRows];
        uint16_t minimum = std::numeric_limits<uint16_t>::max();
        for (uint32_t row = 0; row < kk_ablation_detail::kRows; ++row) {
            indices[row] = index(hash, stride, row);
            minimum = std::min(minimum, counters_[indices[row]]);
        }
        for (uint32_t row = 0; row < kk_ablation_detail::kRows; ++row) {
            uint16_t& counter = counters_[indices[row]];
            if (counter == minimum && counter != std::numeric_limits<uint16_t>::max()) ++counter;
        }
    }

    int Query(const char* str) override {
        const uint32_t hash = MurmurHash32(str, KEY_LEN, seed_);
        const uint32_t stride = kk_ablation_detail::stride_hash(hash);
        uint16_t minimum = std::numeric_limits<uint16_t>::max();
        for (uint32_t row = 0; row < kk_ablation_detail::kRows; ++row) {
            minimum = std::min(minimum, counters_[index(hash, stride, row)]);
        }
        return minimum;
    }

private:
    uint32_t index(uint32_t hash, uint32_t stride, uint32_t row) const {
        return row * width_ + kk_ablation_detail::fast_range(hash + row * stride, width_);
    }

    uint32_t seed_;
    uint32_t width_;
    std::vector<uint16_t> counters_;
};

class Residual_Only_Sketch : public Sketch {
public:
    Residual_Only_Sketch(int bytes, int, int hash_seed = 1000)
        : seed_(static_cast<uint32_t>(hash_seed)) {
        width_ = std::max(static_cast<uint32_t>(std::max(bytes, 0)) /
                              (static_cast<uint32_t>(sizeof(int16_t)) * kk_ablation_detail::kRows),
                          kk_ablation_detail::kMinWidth);
        counters_.assign(width_ * kk_ablation_detail::kRows, 0);
    }

    void Insert(const char* str) override {
        const uint32_t hash = MurmurHash32(str, KEY_LEN, seed_);
        const uint32_t stride = kk_ablation_detail::stride_hash(hash);
        for (uint32_t row = 0; row < kk_ablation_detail::kRows; ++row) {
            int16_t& counter = counters_[index(hash, stride, row)];
            const int sign = kk_ablation_detail::residual_sign(hash, stride, row);
            if (sign > 0 && counter != std::numeric_limits<int16_t>::max()) ++counter;
            if (sign < 0 && counter != std::numeric_limits<int16_t>::min()) --counter;
        }
    }

    int Query(const char* str) override {
        const uint32_t hash = MurmurHash32(str, KEY_LEN, seed_);
        const uint32_t stride = kk_ablation_detail::stride_hash(hash);
        int32_t values[kk_ablation_detail::kRows];
        for (uint32_t row = 0; row < kk_ablation_detail::kRows; ++row) {
            values[row] = kk_ablation_detail::residual_sign(hash, stride, row) *
                          static_cast<int32_t>(counters_[index(hash, stride, row)]);
        }
        std::sort(values, values + kk_ablation_detail::kRows);
        return values[1];
    }

private:
    uint32_t index(uint32_t hash, uint32_t stride, uint32_t row) const {
        return row * width_ + kk_ablation_detail::fast_range(
            hash + (row + kk_ablation_detail::kRows) * stride, width_);
    }

    uint32_t seed_;
    uint32_t width_;
    std::vector<int16_t> counters_;
};

#endif  // TAILOREDSKETCH_CODE_KK_ABLATION_SKETCHES_H
