#ifndef TAILOREDSKETCH_CODE_STABLE_SKETCH_H
#define TAILOREDSKETCH_CODE_STABLE_SKETCH_H

#include "MurmurHash.h"
#include "Sketch.h"
#include "params.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

class Stable_Sketch : public Sketch {
private:
    struct Cell {
        uint32_t fp = 0;
        int32_t count = 0;
        int32_t stability = 0;
    };

    uint32_t seed = 0;
    uint32_t fp_seed = 0;
    uint32_t rows = 0;
    uint32_t row_width = 0;
    std::mt19937 rng;
    std::vector<uint32_t> row_seeds;
    std::vector<Cell> cells;

    static uint32_t normalize_seed(int seed_value) {
        return static_cast<uint32_t>(seed_value);
    }

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

    static std::vector<uint32_t> derive_seeds(uint32_t base_seed, uint32_t count) {
        std::vector<uint32_t> seeds(count);
        uint32_t x = base_seed ^ 0x9e3779b9U;
        for (uint32_t i = 0; i < count; ++i) {
            x = mix32(x + 0x9e3779b9U + i * 0x85ebca6bU);
            seeds[i] = x == 0U ? 1U : x;
        }
        return seeds;
    }

    static uint32_t fingerprint(const char* str, uint32_t fp_seed_value) {
        const uint64_t h1 = hash64(str, fp_seed_value);
        const uint64_t h2 = hash64(str, fp_seed_value ^ 0xDEADU);
        uint32_t fp = static_cast<uint32_t>((h1 ^ (h2 >> 32U)) & 0xFFFFFFFFU);
        return fp == 0U ? 1U : fp;
    }

    uint32_t bucket_index(const char* str, uint32_t row) const {
        const uint64_t h = hash64(str, row_seeds[row]);
        return static_cast<uint32_t>(h % row_width);
    }

    Cell& at(uint32_t row, uint32_t col) {
        return cells[row * row_width + col];
    }

    const Cell& at(uint32_t row, uint32_t col) const {
        return cells[row * row_width + col];
    }

    static int32_t clamp_int32(uint64_t value) {
        return value > static_cast<uint64_t>(std::numeric_limits<int32_t>::max())
            ? std::numeric_limits<int32_t>::max()
            : static_cast<int32_t>(value);
    }

public:
    Stable_Sketch(int bytes, int d, int hash_seed = 1000) {
        seed = normalize_seed(hash_seed);
        fp_seed = mix32(seed ^ 0xDEADU);
        rows = static_cast<uint32_t>(std::max(1, d));

        const uint32_t safe_bytes = std::max(bytes, 4096);
        const uint32_t cell_bytes = static_cast<uint32_t>(sizeof(uint32_t) + sizeof(int32_t) + sizeof(int32_t));
        row_width = std::max(64U, safe_bytes / (rows * cell_bytes));

        rng.seed(seed);
        row_seeds = derive_seeds(seed, rows);
        cells.assign(static_cast<size_t>(rows) * row_width, Cell{});
    }

    void Insert(const char* str) override {
        const uint32_t fp = fingerprint(str, fp_seed);
        uint32_t min_count = std::numeric_limits<uint32_t>::max();
        int min_row = -1;
        int min_col = -1;

        for (uint32_t row = 0; row < rows; ++row) {
            const uint32_t col = bucket_index(str, row);
            Cell& cell = at(row, col);
            if (cell.fp == 0U && cell.count == 0) {
                cell.fp = fp;
                cell.count = 1;
                cell.stability += 1;
                return;
            }
            if (cell.fp == fp) {
                if (cell.count < std::numeric_limits<int32_t>::max()) ++cell.count;
                cell.stability += 1;
                return;
            }
            const uint32_t current = cell.count < 0 ? 0U : static_cast<uint32_t>(cell.count);
            if (current < min_count) {
                min_count = current;
                min_row = static_cast<int>(row);
                min_col = static_cast<int>(col);
            }
        }

        if (min_row < 0 || min_col < 0) return;

        Cell& victim = at(static_cast<uint32_t>(min_row), static_cast<uint32_t>(min_col));
        const int32_t s = std::max(0, victim.stability);
        const int32_t c = std::max(0, victim.count);
        const uint64_t sc = static_cast<uint64_t>(s) * static_cast<uint64_t>(c);
        std::uniform_int_distribution<uint64_t> dist(1U, sc + 1U);
        if (dist(rng) > sc) {
            victim.count -= 1;
            if (victim.count <= 0) {
                victim.fp = fp;
                victim.count = 1;
                victim.stability = std::max(0, s - 1);
            }
        }
    }

    int Query(const char* str) override {
        const uint32_t fp = fingerprint(str, fp_seed);
        uint64_t total = 0;
        for (uint32_t row = 0; row < rows; ++row) {
            const uint32_t col = bucket_index(str, row);
            const Cell& cell = at(row, col);
            if (cell.fp == fp) {
                total += static_cast<uint64_t>(std::max(0, cell.count));
            }
        }
        return static_cast<int>(std::min<uint64_t>(total, static_cast<uint64_t>(std::numeric_limits<int>::max())));
    }
};

#endif
