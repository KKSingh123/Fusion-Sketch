#ifndef TAILOREDSKETCH_CODE_NITRO_SKETCH_H
#define TAILOREDSKETCH_CODE_NITRO_SKETCH_H

#include "MurmurHash.h"
#include "Sketch.h"
#include "params.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

class Nitro_Sketch : public Sketch {
private:
    uint32_t seed;
    uint32_t rows;
    uint32_t width;
    uint32_t sampling_rate;
    uint32_t rng_state;
    std::vector<uint32_t> counters;

    static constexpr uint32_t kRows = 4;
    static constexpr uint32_t kSamplingRate = 16;
    static constexpr uint32_t kMinWidth = 64;

    static uint32_t fast_range(uint32_t hash, uint32_t range) {
        return static_cast<uint32_t>((static_cast<uint64_t>(hash) * range) >> 32);
    }

    uint32_t next_random() {
        uint32_t x = rng_state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        rng_state = x == 0 ? 0x9e3779b9U : x;
        return rng_state;
    }

    bool admit_packet() {
        return fast_range(next_random(), sampling_rate) == 0;
    }

    uint32_t index(const char* str, uint32_t row) const {
        const uint32_t h = MurmurHash32(str, KEY_LEN, seed + row);
        return row * width + fast_range(h, width);
    }

public:
    Nitro_Sketch(int bytes, int, int hash_seed = 1000) {
        seed = static_cast<uint32_t>(hash_seed);
        rows = kRows;
        sampling_rate = kSamplingRate;
        rng_state = seed ^ 0xdeadbeefU;
        if (rng_state == 0) rng_state = 0x9e3779b9U;

        const uint32_t safe_bytes = std::max(bytes, 4096);
        width = safe_bytes / (sizeof(uint32_t) * rows);
        width = std::max(width, kMinWidth);
        counters.assign(width * rows, 0);
    }

    void Insert(const char* str) override {
        if (!admit_packet()) return;
        for (uint32_t row = 0; row < rows; ++row) {
            uint32_t& counter = counters[index(str, row)];
            if (counter != std::numeric_limits<uint32_t>::max()) ++counter;
        }
    }

    int Query(const char* str) override {
        uint32_t estimate = std::numeric_limits<uint32_t>::max();
        for (uint32_t row = 0; row < rows; ++row) {
            estimate = std::min(estimate, counters[index(str, row)]);
        }
        const uint64_t scaled = static_cast<uint64_t>(estimate) * sampling_rate;
        return scaled > static_cast<uint64_t>(std::numeric_limits<int>::max())
            ? std::numeric_limits<int>::max()
            : static_cast<int>(scaled);
    }
};

#endif
