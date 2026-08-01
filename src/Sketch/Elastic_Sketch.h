#ifndef TAILOREDSKETCH_CODE_ELASTIC_SKETCH_H
#define TAILOREDSKETCH_CODE_ELASTIC_SKETCH_H

#include "MurmurHash.h"
#include "Sketch.h"
#include "params.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

class Elastic_Sketch : public Sketch {
private:
    struct HeavyCell {
        std::array<char, KEY_LEN> key;
        uint32_t count;
        bool occupied;
    };

    uint32_t seed;
    uint32_t rows;
    uint32_t light_width;
    uint32_t heavy_count;
    std::vector<uint32_t> light;
    std::vector<HeavyCell> heavy;

    static constexpr uint32_t kRows = 4;
    static constexpr uint32_t kHeavyPercent = 36;
    static constexpr uint32_t kMinWidth = 64;
    static constexpr uint32_t kMinHeavy = 16;

    static uint32_t fast_range(uint32_t hash, uint32_t range) {
        return static_cast<uint32_t>((static_cast<uint64_t>(hash) * range) >> 32);
    }

    uint32_t light_index(const char* str, uint32_t row) const {
        const uint32_t h = MurmurHash32(str, KEY_LEN, seed + 101U + row);
        return row * light_width + fast_range(h, light_width);
    }

    uint32_t heavy_index(const char* str) const {
        const uint32_t h = MurmurHash32(str, KEY_LEN, seed + 17U);
        return fast_range(h, heavy_count);
    }

    static bool same_key(const HeavyCell& cell, const char* str) {
        return cell.occupied && std::memcmp(cell.key.data(), str, KEY_LEN) == 0;
    }

    void set_key(HeavyCell& cell, const char* str, uint32_t count) {
        std::memcpy(cell.key.data(), str, KEY_LEN);
        cell.count = count;
        cell.occupied = true;
    }

    void light_update(const char* str) {
        uint32_t idx[kRows];
        uint32_t minimum = std::numeric_limits<uint32_t>::max();
        for (uint32_t row = 0; row < rows; ++row) {
            idx[row] = light_index(str, row);
            minimum = std::min(minimum, light[idx[row]]);
        }
        for (uint32_t row = 0; row < rows; ++row) {
            uint32_t& counter = light[idx[row]];
            if (counter == minimum && counter != std::numeric_limits<uint32_t>::max()) ++counter;
        }
    }

    uint32_t light_query(const char* str) const {
        uint32_t answer = std::numeric_limits<uint32_t>::max();
        for (uint32_t row = 0; row < rows; ++row) {
            answer = std::min(answer, light[light_index(str, row)]);
        }
        return answer;
    }

public:
    Elastic_Sketch(int bytes, int, int hash_seed = 1000) {
        seed = static_cast<uint32_t>(hash_seed);
        rows = kRows;

        const uint32_t safe_bytes = std::max(bytes, 4096);
        const uint32_t heavy_bytes = safe_bytes * kHeavyPercent / 100;
        const uint32_t light_bytes = safe_bytes - heavy_bytes;

        heavy_count = heavy_bytes / sizeof(HeavyCell);
        heavy_count = std::max(heavy_count, kMinHeavy);
        heavy.assign(heavy_count, HeavyCell{{}, 0, false});

        light_width = light_bytes / (sizeof(uint32_t) * rows);
        light_width = std::max(light_width, kMinWidth);
        light.assign(light_width * rows, 0);
    }

    void Insert(const char* str) override {
        HeavyCell& cell = heavy[heavy_index(str)];
        if (!cell.occupied) {
            set_key(cell, str, 1);
            return;
        }
        if (same_key(cell, str)) {
            if (cell.count != std::numeric_limits<uint32_t>::max()) ++cell.count;
            return;
        }

        if (cell.count > 1) {
            --cell.count;
        } else {
            set_key(cell, str, 1);
        }
        light_update(str);
    }

    int Query(const char* str) override {
        const HeavyCell& cell = heavy[heavy_index(str)];
        const uint32_t heavy_estimate = same_key(cell, str) ? cell.count : 0;
        return static_cast<int>(heavy_estimate + light_query(str));
    }
};

#endif
