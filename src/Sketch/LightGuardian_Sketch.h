#ifndef TAILOREDSKETCH_CODE_LIGHTGUARDIAN_SKETCH_H
#define TAILOREDSKETCH_CODE_LIGHTGUARDIAN_SKETCH_H

#include "MurmurHash.h"
#include "Sketch.h"
#include "params.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

class LightGuardian_Sketch : public Sketch {
private:
    uint32_t seed = 0;
    uint32_t num_hash = 0;
    uint32_t row_size = 0;
    std::vector<uint32_t> counters;
    std::vector<uint32_t> hash_seeds;

    static uint32_t mix32(uint32_t x) {
        x ^= x >> 16;
        x *= 0x7feb352dU;
        x ^= x >> 15;
        x *= 0x846ca68bU;
        x ^= x >> 16;
        return x;
    }

    static uint64_t hash64(const char* str, uint32_t seed_value) {
        return MurmurHash64B(str, KEY_LEN, seed_value);
    }

    uint32_t index(const char* str, uint32_t row) const {
        const uint64_t h = hash64(str, hash_seeds[row]);
        return row * row_size + static_cast<uint32_t>(h % row_size);
    }

    static void inc_sat(uint32_t& value) {
        if (value != std::numeric_limits<uint32_t>::max()) ++value;
    }

public:
    LightGuardian_Sketch(int bytes, int d, int hash_seed = 1000) {
        seed = static_cast<uint32_t>(hash_seed);
        num_hash = static_cast<uint32_t>(std::max(1, d));
        const uint32_t safe_bytes = std::max(bytes, 4096);
        row_size = std::max(64U, safe_bytes / (num_hash * static_cast<uint32_t>(sizeof(uint32_t))));
        counters.assign(static_cast<size_t>(num_hash) * row_size, 0U);
        hash_seeds.resize(num_hash);
        uint32_t x = mix32(seed ^ 0x9e3779b9U);
        for (uint32_t i = 0; i < num_hash; ++i) {
            x = mix32(x + 0x85ebca6bU + i * 0x27d4eb2dU);
            hash_seeds[i] = x == 0U ? (i + 1U) : x;
        }
    }

    void Insert(const char* str) override {
        std::vector<uint32_t> idxs(num_hash);
        uint32_t min_value = std::numeric_limits<uint32_t>::max();

        for (uint32_t i = 0; i < num_hash; ++i) {
            idxs[i] = index(str, i);
            min_value = std::min(min_value, counters[idxs[i]]);
        }

        for (uint32_t i = 0; i < num_hash; ++i) {
            uint32_t& cur = counters[idxs[i]];
            if (cur == min_value) inc_sat(cur);
        }
    }

    int Query(const char* str) override {
        uint32_t estimate = std::numeric_limits<uint32_t>::max();
        for (uint32_t i = 0; i < num_hash; ++i) {
            estimate = std::min(estimate, counters[index(str, i)]);
        }
        return static_cast<int>(std::min<uint32_t>(estimate, static_cast<uint32_t>(std::numeric_limits<int>::max())));
    }
};

#endif
