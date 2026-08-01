#ifndef TAILOREDSKETCH_CODE_UNIVMON_SKETCH_H
#define TAILOREDSKETCH_CODE_UNIVMON_SKETCH_H

#include "MurmurHash.h"
#include "Sketch.h"
#include "params.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

class UnivMon_Sketch : public Sketch {
private:
    uint32_t seed;
    uint32_t levels;
    uint32_t rows;
    uint32_t width;
    std::vector<int32_t> counters;

    static constexpr uint32_t kLevels = 8;
    static constexpr uint32_t kRows = 2;
    static constexpr uint32_t kMinWidth = 64;

    static uint32_t fast_range(uint32_t hash, uint32_t range) {
        return static_cast<uint32_t>((static_cast<uint64_t>(hash) * range) >> 32);
    }

    uint32_t offset(uint32_t level, uint32_t row, uint32_t index) const {
        return (level * rows + row) * width + index;
    }

    bool sampled_at(const char* str, uint32_t level) const {
        if (level == 0) return true;
        const uint64_t h = MurmurHash64B(str, KEY_LEN, seed + 1009U + level);
        return (h >> (64U - level)) == 0;
    }

    uint32_t bucket(const char* str, uint32_t level, uint32_t row) const {
        const uint32_t h = MurmurHash32(str, KEY_LEN, seed + 4099U + level * rows + row);
        return fast_range(h, width);
    }

    int sign(const char* str, uint32_t level, uint32_t row) const {
        const uint32_t h = MurmurHash32(str, KEY_LEN, seed + 8191U + level * rows + row);
        return (h & 1U) ? 1 : -1;
    }

    int32_t level_estimate(const char* str, uint32_t level) const {
        const uint32_t i0 = bucket(str, level, 0);
        const int32_t e0 = counters[offset(level, 0, i0)] * sign(str, level, 0);
        if (rows == 1) return e0;

        const uint32_t i1 = bucket(str, level, 1);
        const int32_t e1 = counters[offset(level, 1, i1)] * sign(str, level, 1);
        return (e0 + e1) / 2;
    }

public:
    UnivMon_Sketch(int bytes, int, int hash_seed = 1000) {
        seed = static_cast<uint32_t>(hash_seed);
        levels = kLevels;
        rows = kRows;
        const uint32_t safe_bytes = std::max(bytes, 4096);
        width = safe_bytes / (sizeof(int32_t) * levels * rows);
        width = std::max(width, kMinWidth);
        counters.assign(levels * rows * width, 0);
    }

    void Insert(const char* str) override {
        for (uint32_t level = 0; level < levels; ++level) {
            if (!sampled_at(str, level)) break;
            for (uint32_t row = 0; row < rows; ++row) {
                const uint32_t idx = bucket(str, level, row);
                counters[offset(level, row, idx)] += sign(str, level, row);
            }
        }
    }

    int Query(const char* str) override {
        uint32_t best = 0;
        for (uint32_t level = 0; level < levels; ++level) {
            if (!sampled_at(str, level)) break;
            best = level;
        }
        int64_t estimate = static_cast<int64_t>(level_estimate(str, best)) << best;
        if (estimate < 0) estimate = 0;
        if (estimate > static_cast<int64_t>(std::numeric_limits<int>::max())) {
            estimate = std::numeric_limits<int>::max();
        }
        return static_cast<int>(estimate);
    }
};

#endif
