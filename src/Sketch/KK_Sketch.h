#ifndef TAILOREDSKETCH_CODE_KK_SKETCH_H
#define TAILOREDSKETCH_CODE_KK_SKETCH_H

#include "MurmurHash.h"
#include "Sketch.h"
#include "params.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

class KK_Sketch : public Sketch {
public:
    struct QueryRouteStats {
        uint64_t cu_minimum = 0;
        uint64_t residual_minimum = 0;
        uint64_t residual_nonpositive_fallback = 0;
    };

private:
    uint32_t seed;
    uint32_t upper_width;
    uint32_t residual_width;
    uint64_t packet_count;
    std::vector<uint16_t> upper;
    std::vector<int16_t> residual;
    QueryRouteStats query_route_stats;

    static constexpr uint32_t kUpperRows = 3;
    static constexpr uint32_t kResidualRows = 3;
    static constexpr uint32_t kSmallUpperPercent = 30;
    static constexpr uint32_t kLargeUpperPercent = 20;
    static constexpr uint32_t kMinWidth = 64;

    static uint32_t fast_range(uint32_t hash, uint32_t range) {
        return static_cast<uint32_t>((static_cast<uint64_t>(hash) * range) >> 32);
    }

    static uint32_t mix32(uint32_t x) {
        x ^= x >> 16;
        x *= 0x7feb352dU;
        x ^= x >> 15;
        x *= 0x846ca68bU;
        x ^= x >> 16;
        return x;
    }

    static uint32_t stride_hash(uint32_t hash) {
        return mix32(hash ^ 0x9e3779b9U) | 1U;
    }

    static int residual_sign(uint32_t hash, uint32_t stride, uint32_t row) {
        return (((hash >> (row + 9U)) ^ (stride >> (row + 3U))) & 1U) ? 1 : -1;
    }

    uint32_t upper_index(uint32_t hash, uint32_t stride, uint32_t row) const {
        return row * upper_width + fast_range(hash + row * stride, upper_width);
    }

    uint32_t residual_index(uint32_t hash, uint32_t stride, uint32_t row) const {
        return row * residual_width + fast_range(hash + (row + kUpperRows) * stride, residual_width);
    }

    static void inc_sat(uint16_t& x) {
        if (x != std::numeric_limits<uint16_t>::max()) ++x;
    }

    static void add_sat(int16_t& x, int delta) {
        if (delta > 0) {
            if (x != std::numeric_limits<int16_t>::max()) ++x;
        } else {
            if (x != std::numeric_limits<int16_t>::min()) --x;
        }
    }

    uint32_t upper_query(uint32_t hash, uint32_t stride) const {
        const uint32_t i0 = upper_index(hash, stride, 0);
        const uint32_t i1 = upper_index(hash, stride, 1);
        const uint32_t i2 = upper_index(hash, stride, 2);
        return std::min<uint32_t>(upper[i0], std::min<uint32_t>(upper[i1], upper[i2]));
    }

    int32_t median3(int32_t a, int32_t b, int32_t c) const {
        if (a > b) std::swap(a, b);
        if (b > c) std::swap(b, c);
        if (a > b) std::swap(a, b);
        return b;
    }

    int32_t residual_query(uint32_t hash, uint32_t stride) const {
        const int32_t v0 = residual_sign(hash, stride, 0) * static_cast<int32_t>(residual[residual_index(hash, stride, 0)]);
        const int32_t v1 = residual_sign(hash, stride, 1) * static_cast<int32_t>(residual[residual_index(hash, stride, 1)]);
        const int32_t v2 = residual_sign(hash, stride, 2) * static_cast<int32_t>(residual[residual_index(hash, stride, 2)]);
        return median3(v0, v1, v2);
    }

public:
    KK_Sketch(int bytes, int, int hash_seed = 1000) {
        seed = static_cast<uint32_t>(hash_seed);
        packet_count = 0;

        const uint32_t safe_bytes = std::max(bytes, 4096);
        const uint32_t upper_percent = safe_bytes < (300U * 1024U) ? kSmallUpperPercent : kLargeUpperPercent;
        const uint32_t upper_bytes = safe_bytes * upper_percent / 100;
        const uint32_t residual_bytes = safe_bytes - upper_bytes;

        upper_width = upper_bytes / (sizeof(uint16_t) * kUpperRows);
        upper_width = std::max(upper_width, kMinWidth);
        upper.assign(upper_width * kUpperRows, 0);

        residual_width = residual_bytes / (sizeof(int16_t) * kResidualRows);
        residual_width = std::max(residual_width, kMinWidth);
        residual.assign(residual_width * kResidualRows, 0);
    }

    void Insert(const char* str) override {
        ++packet_count;
        const uint32_t hash = MurmurHash32(str, KEY_LEN, seed);
        const uint32_t stride = stride_hash(hash);

        const uint32_t u0 = upper_index(hash, stride, 0);
        const uint32_t u1 = upper_index(hash, stride, 1);
        const uint32_t u2 = upper_index(hash, stride, 2);
        const uint16_t minimum = std::min<uint16_t>(upper[u0], std::min<uint16_t>(upper[u1], upper[u2]));
        if (upper[u0] == minimum) inc_sat(upper[u0]);
        if (upper[u1] == minimum) inc_sat(upper[u1]);
        if (upper[u2] == minimum) inc_sat(upper[u2]);

        add_sat(residual[residual_index(hash, stride, 0)], residual_sign(hash, stride, 0));
        add_sat(residual[residual_index(hash, stride, 1)], residual_sign(hash, stride, 1));
        add_sat(residual[residual_index(hash, stride, 2)], residual_sign(hash, stride, 2));
    }

    int Query(const char* str) override {
        const uint32_t hash = MurmurHash32(str, KEY_LEN, seed);
        const uint32_t stride = stride_hash(hash);
        const uint32_t upper_estimate = upper_query(hash, stride);
        const int32_t residual_estimate = residual_query(hash, stride);
        if (residual_estimate <= 0) {
            ++query_route_stats.residual_nonpositive_fallback;
            return static_cast<int>(std::min<uint32_t>(upper_estimate, 1));
        }
        if (upper_estimate <= static_cast<uint32_t>(residual_estimate)) {
            ++query_route_stats.cu_minimum;
            return static_cast<int>(upper_estimate);
        }
        ++query_route_stats.residual_minimum;
        return residual_estimate;
    }

    void ResetQueryRouteStats() { query_route_stats = QueryRouteStats{}; }
    QueryRouteStats GetQueryRouteStats() const { return query_route_stats; }
};

#endif
