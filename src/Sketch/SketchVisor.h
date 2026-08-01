#ifndef TAILOREDSKETCH_CODE_SKETCHVISOR_H
#define TAILOREDSKETCH_CODE_SKETCHVISOR_H

#include "MurmurHash.h"
#include "Sketch.h"
#include "params.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_set>
#include <vector>

class SketchVisor : public Sketch {
private:
    uint32_t fast_width;
    uint32_t slow_width;
    uint32_t rows;
    uint32_t fast_seed;
    uint32_t slow_seed;
    std::vector<std::string> fast_keys;
    std::vector<uint8_t> fast_used;
    std::vector<uint32_t> fast_count;
    std::vector<uint32_t> slow;
    std::unordered_set<std::string> slow_set;

    static constexpr uint32_t kFastRatioNumerator = 1;
    static constexpr uint32_t kFastRatioDenominator = 4;

    static void add_sat(uint32_t& value, uint32_t delta = 1) {
        if (value > std::numeric_limits<uint32_t>::max() - delta) {
            value = std::numeric_limits<uint32_t>::max();
        } else {
            value += delta;
        }
    }

    static std::string key_from(const char* str) {
        return std::string(str, KEY_LEN);
    }

    uint32_t fast_index(const std::string& key) const {
        return MurmurHash32(key.data(), KEY_LEN, fast_seed) % fast_width;
    }

    uint32_t slow_index(const std::string& key, uint32_t row) const {
        return row * slow_width + MurmurHash32(key.data(), KEY_LEN, slow_seed + row) % slow_width;
    }

    void slow_update(const std::string& key) {
        uint32_t minimum = std::numeric_limits<uint32_t>::max();
        std::vector<uint32_t> idx(rows);
        for (uint32_t row = 0; row < rows; ++row) {
            idx[row] = slow_index(key, row);
            minimum = std::min(minimum, slow[idx[row]]);
        }
        for (uint32_t row = 0; row < rows; ++row) {
            if (slow[idx[row]] == minimum) add_sat(slow[idx[row]]);
        }
    }

    void slow_inject(const std::string& key, uint32_t count) {
        for (uint32_t row = 0; row < rows; ++row) {
            add_sat(slow[slow_index(key, row)], count);
        }
    }

    uint32_t slow_query(const std::string& key) const {
        uint32_t estimate = std::numeric_limits<uint32_t>::max();
        for (uint32_t row = 0; row < rows; ++row) {
            estimate = std::min(estimate, slow[slow_index(key, row)]);
        }
        return estimate;
    }

public:
    SketchVisor(int bytes, int d, int hash_seed = 1000) {
        rows = std::max(1, d);
        const uint32_t safe_bytes = std::max(bytes, 4096);
        const uint32_t denominator = sizeof(uint32_t) *
            (kFastRatioNumerator + rows * (kFastRatioDenominator - kFastRatioNumerator));
        const uint32_t row_width = std::max(1U, safe_bytes * kFastRatioDenominator / denominator);

        fast_width = std::max(1U, row_width * kFastRatioNumerator / kFastRatioDenominator);
        slow_width = std::max(1U, row_width - fast_width);
        fast_seed = static_cast<uint32_t>(hash_seed + rows);
        slow_seed = static_cast<uint32_t>(hash_seed);

        fast_keys.assign(fast_width, std::string());
        fast_used.assign(fast_width, 0);
        fast_count.assign(fast_width, 0);
        slow.assign(rows * slow_width, 0);
    }

    void Insert(const char* str) override {
        const std::string key = key_from(str);
        if (slow_set.count(key)) {
            slow_update(key);
            return;
        }

        const uint32_t idx = fast_index(key);
        if (!fast_used[idx]) {
            fast_used[idx] = 1;
            fast_keys[idx] = key;
            fast_count[idx] = 1;
        } else if (fast_keys[idx] == key) {
            add_sat(fast_count[idx]);
        } else {
            if (fast_count[idx] > 0) slow_inject(fast_keys[idx], fast_count[idx]);
            slow_set.insert(fast_keys[idx]);
            fast_keys[idx] = key;
            fast_count[idx] = 1;
        }
    }

    int Query(const char* str) override {
        const std::string key = key_from(str);
        const uint32_t idx = fast_index(key);
        uint32_t estimate = slow_query(key);
        if (fast_used[idx] && fast_keys[idx] == key) add_sat(estimate, fast_count[idx]);
        return static_cast<int>(std::min<uint32_t>(estimate, static_cast<uint32_t>(std::numeric_limits<int>::max())));
    }
};

#endif
