#include "Choose_Ske.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using Clock = std::chrono::steady_clock;

struct Flow {
    std::string key;
    int real;
};

struct Method {
    std::string name;
    int id;
};

struct Metrics {
    std::string method;
    int target_memory_bytes = 0;
    int actual_memory_bytes = 0;
    double aae = 0.0;
    double are = 0.0;
    double are_hh = 0.0;
    double f1 = 0.0;
    double accuracy = 0.0;
    double f1_hh = 0.0;
    double f1_hc = 0.0;
    double re = 0.0;
    double wmre = 0.0;
    double insert_mpps = 0.0;
    double query_mqps = 0.0;
    double precision = 0.0;
    double recall = 0.0;
    double hc_precision = 0.0;
    double hc_recall = 0.0;
    uint64_t true_positive = 0;
    uint64_t false_positive = 0;
    uint64_t false_negative = 0;
    uint64_t hc_true_positive = 0;
    uint64_t hc_false_positive = 0;
    uint64_t hc_false_negative = 0;
    uint64_t query_cu_minimum = 0;
    uint64_t query_residual_minimum = 0;
    uint64_t query_residual_nonpositive_fallback = 0;
};

struct MemoryConfig {
    int constructor_arg = 0;
    int actual_bytes = 0;
};

static std::string pad_key(const char* data, int len) {
    std::string key(data, data + len);
    if (static_cast<int>(key.size()) < KEY_LEN) key.resize(KEY_LEN, '\0');
    return key;
}

static int actual_bytes_for_method(int constructor_arg, int d, int method_id) {
    switch (method_id) {
        case 0: {
            const int hash_size = d == 2 ? 0x20 : 0x40;
            const int bias_range = std::min((hash_size - static_cast<int>(std::ceil(std::log2(constructor_arg)))) / (d - 1), 0x08);
            const int allocated_width = (constructor_arg - (((d - 1) << bias_range) + 0x10)) >> bias_range << bias_range;
            return allocated_width + ((d - 1) << bias_range) + 0x10;
        }
        case 10:
        case 20:
        case 30:
            return (constructor_arg / 4 / d) * d * static_cast<int>(sizeof(int));
        case 40: {
            const int w0 = constructor_arg * 4 / 7;
            const int w1 = constructor_arg * 2 / 7 / 2;
            const int w2 = constructor_arg * 1 / 7 / 4;
            return (w0 + w1 + w2) * static_cast<int>(sizeof(unsigned int));
        }
        case 50:
        case 51: {
            const int w0 = constructor_arg / 3;
            const int w1 = constructor_arg / 3;
            const int w2 = constructor_arg / 3;
            return std::max(1, w0) + std::max(1, w1) + std::max(1, w2);
        }
        case 60: {
            const int safe_bytes = std::max(constructor_arg, 4096);
            const int upper_percent = safe_bytes < (300 * 1024) ? 30 : 20;
            const int upper_bytes = safe_bytes * upper_percent / 100;
            const int residual_bytes = safe_bytes - upper_bytes;
            const int upper_width = std::max(upper_bytes / (static_cast<int>(sizeof(uint16_t)) * 3), 64);
            const int residual_width = std::max(residual_bytes / (static_cast<int>(sizeof(int16_t)) * 3), 64);
            return upper_width * 3 * static_cast<int>(sizeof(uint16_t))
                 + residual_width * 3 * static_cast<int>(sizeof(int16_t));
        }
        case 61: {
            const int safe_bytes = std::max(constructor_arg, 4096);
            const int rows = std::max(1, d);
            const int cell_bytes = static_cast<int>(sizeof(uint32_t) + sizeof(int32_t) + sizeof(int32_t));
            const int row_width = std::max(safe_bytes / (rows * cell_bytes), 64);
            return rows * row_width * cell_bytes;
        }
        case 62: {
            const int safe_bytes = std::max(constructor_arg, 4096);
            const int rows = std::max(1, d);
            const int row_width = std::max(safe_bytes / (rows * static_cast<int>(sizeof(uint32_t))), 64);
            return rows * row_width * static_cast<int>(sizeof(uint32_t))
                 + rows * static_cast<int>(sizeof(uint32_t));
        }
        case 63:
            return std::max(constructor_arg / (static_cast<int>(sizeof(uint16_t)) * 3), 64)
                 * 3 * static_cast<int>(sizeof(uint16_t));
        case 64:
            return std::max(constructor_arg / (static_cast<int>(sizeof(int16_t)) * 3), 64)
                 * 3 * static_cast<int>(sizeof(int16_t));
        case 70: {
            const int safe_bytes = std::max(constructor_arg, 4096);
            const int heavy_bytes = safe_bytes * 36 / 100;
            const int light_bytes = safe_bytes - heavy_bytes;
            const int heavy_count = std::max(heavy_bytes / static_cast<int>(KEY_LEN + sizeof(uint32_t) + sizeof(bool)), 16);
            const int light_width = std::max(light_bytes / (static_cast<int>(sizeof(uint32_t)) * 4), 64);
            return heavy_count * static_cast<int>(KEY_LEN + sizeof(uint32_t) + sizeof(bool))
                 + light_width * 4 * static_cast<int>(sizeof(uint32_t));
        }
        case 71: {
            const int safe_bytes = std::max(constructor_arg, 4096);
            const int width = std::max(safe_bytes / (static_cast<int>(sizeof(int32_t)) * 8 * 2), 64);
            return width * 8 * 2 * static_cast<int>(sizeof(int32_t));
        }
        case 72: {
            const int safe_bytes = std::max(constructor_arg, 4096);
            const int width = std::max(safe_bytes / (static_cast<int>(sizeof(uint32_t)) * 4), 64);
            return width * 4 * static_cast<int>(sizeof(uint32_t));
        }
        case 73: {
            const int safe_bytes = std::max(constructor_arg, 4096);
            const int row_width = std::max(1, safe_bytes / (static_cast<int>(sizeof(uint32_t)) * (1 + 3 * d) / 4));
            const int fast_width = std::max(1, row_width / 4);
            const int slow_width = std::max(1, row_width - fast_width);
            return (fast_width + d * slow_width) * static_cast<int>(sizeof(uint32_t));
        }
        default:
            return constructor_arg;
    }
}

static MemoryConfig normalize_memory_budget(int target_bytes, int d, int method_id) {
    int low = 1;
    int high = std::max(target_bytes, 4096);
    int best_arg = low;
    int best_actual = actual_bytes_for_method(low, d, method_id);
    while (low <= high) {
        const int mid = low + (high - low) / 2;
        const int actual = actual_bytes_for_method(mid, d, method_id);
        if (actual <= target_bytes) {
            best_arg = mid;
            best_actual = actual;
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    return MemoryConfig{best_arg, best_actual};
}

static bool load_dataset(const std::string& path, int record_len, std::vector<std::string>& items, std::vector<Flow>& flows) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    std::unordered_map<std::string, int> freq;
    std::string buffer(record_len, '\0');
    while (in.read(buffer.data(), record_len)) {
        std::string key = pad_key(buffer.data(), record_len);
        items.push_back(key);
        ++freq[key];
    }

    flows.reserve(freq.size());
    for (const auto& entry : freq) flows.push_back(Flow{entry.first, entry.second});
    std::sort(flows.begin(), flows.end(), [](const Flow& a, const Flow& b) {
        if (a.real != b.real) return a.real > b.real;
        return a.key < b.key;
    });
    return !items.empty();
}

static std::vector<Flow> flows_from_freq(const std::unordered_map<std::string, int>& freq) {
    std::vector<Flow> flows;
    flows.reserve(freq.size());
    for (const auto& entry : freq) flows.push_back(Flow{entry.first, entry.second});
    std::sort(flows.begin(), flows.end(), [](const Flow& a, const Flow& b) {
        if (a.real != b.real) return a.real > b.real;
        return a.key < b.key;
    });
    return flows;
}

static void build_change_flows(const std::vector<std::string>& first_items,
                               const std::vector<std::string>& second_items,
                               std::vector<Flow>& change_flows,
                               int& total_true_change) {
    std::unordered_map<std::string, int> first_freq;
    std::unordered_map<std::string, int> second_freq;
    first_freq.reserve(first_items.size());
    second_freq.reserve(second_items.size());
    for (const auto& key : first_items) ++first_freq[key];
    for (const auto& key : second_items) ++second_freq[key];

    std::unordered_map<std::string, int> change_freq;
    change_freq.reserve(first_freq.size() + second_freq.size());
    total_true_change = 0;
    for (const auto& entry : first_freq) {
        const int second = second_freq.count(entry.first) ? second_freq[entry.first] : 0;
        const int change = std::abs(second - entry.second);
        if (change > 0) {
            change_freq[entry.first] = change;
            total_true_change += change;
        }
    }
    for (const auto& entry : second_freq) {
        if (first_freq.count(entry.first)) continue;
        change_freq[entry.first] = entry.second;
        total_true_change += entry.second;
    }
    change_flows = flows_from_freq(change_freq);
}

static void split_windows(const std::vector<std::string>& items,
                          std::vector<std::string>& first_items,
                          std::vector<std::string>& second_items,
                          std::vector<Flow>& change_flows,
                          int& total_true_change) {
    const size_t mid = items.size() / 2;
    first_items.assign(items.begin(), items.begin() + mid);
    second_items.assign(items.begin() + mid, items.end());
    build_change_flows(first_items, second_items, change_flows, total_true_change);
}

static double entropy_from_hist(const std::unordered_map<int, int>& hist, double total_packets) {
    if (total_packets <= 0.0) return 0.0;
    double entropy = 0.0;
    for (const auto& entry : hist) {
        const double packets_in_bin = static_cast<double>(entry.first) * entry.second;
        if (packets_in_bin <= 0.0) continue;
        const double p = packets_in_bin / total_packets;
        entropy -= p * std::log2(p);
    }
    return entropy;
}

static std::unordered_set<std::string> insert_window_and_record_candidates(Sketch& sketch,
                                                                           const std::vector<std::string>& items,
                                                                           int threshold) {
    std::unordered_set<std::string> candidates;
    for (const auto& key : items) {
        sketch.Insert(key.c_str());
        if (sketch.Query(key.c_str()) >= threshold) candidates.insert(key);
    }
    return candidates;
}

static Metrics evaluate(const Method& method,
                        int target_memory_bytes,
                        int constructor_memory_arg,
                        int actual_memory_bytes,
                        int d,
                        int seed,
                        int heavy_threshold,
                        int change_threshold,
                        const std::vector<std::string>& items,
                        const std::vector<Flow>& flows,
                        const std::vector<std::string>& first_items,
                        const std::vector<std::string>& second_items,
                        const std::vector<Flow>& change_flows) {
    std::unique_ptr<Sketch> sketch(Choose_Sketch(constructor_memory_arg, d, seed, method.id));
    if (!sketch) throw std::runtime_error("unknown sketch id");

    const auto insert_begin = Clock::now();
    for (const auto& key : items) sketch->Insert(key.c_str());
    const auto insert_end = Clock::now();

    std::vector<int> estimates;
    estimates.reserve(flows.size());
    KK_Sketch* kk_sketch = dynamic_cast<KK_Sketch*>(sketch.get());
    if (kk_sketch) kk_sketch->ResetQueryRouteStats();
    const auto query_begin = Clock::now();
    for (const auto& flow : flows) estimates.push_back(std::max(0, sketch->Query(flow.key.c_str())));
    const auto query_end = Clock::now();

    Metrics m;
    m.method = method.name;
    m.target_memory_bytes = target_memory_bytes;
    m.actual_memory_bytes = actual_memory_bytes;
    if (kk_sketch) {
        const KK_Sketch::QueryRouteStats stats = kk_sketch->GetQueryRouteStats();
        m.query_cu_minimum = stats.cu_minimum;
        m.query_residual_minimum = stats.residual_minimum;
        m.query_residual_nonpositive_fallback = stats.residual_nonpositive_fallback;
    }

    std::unordered_map<int, int> real_hist;
    std::unordered_map<int, int> est_hist;
    real_hist.reserve(flows.size());
    est_hist.reserve(flows.size());

    uint64_t true_positive = 0, false_positive = 0, false_negative = 0;
    uint64_t real_heavy_count = 0;
    double real_packets = 0.0;
    double est_packets = 0.0;

    for (size_t i = 0; i < flows.size(); ++i) {
        const int real = flows[i].real;
        const int est = estimates[i];
        const int diff = std::abs(est - real);
        m.aae += diff;
        m.are += static_cast<double>(diff) / std::max(1, real);

        const bool real_heavy = real >= heavy_threshold;
        const bool est_heavy = est >= heavy_threshold;
        if (real_heavy) {
            m.are_hh += static_cast<double>(diff) / std::max(1, real);
            ++real_heavy_count;
        }
        if (real_heavy && est_heavy) ++true_positive;
        if (!real_heavy && est_heavy) ++false_positive;
        if (real_heavy && !est_heavy) ++false_negative;

        ++real_hist[real];
        ++est_hist[est];
        real_packets += real;
        est_packets += est;
    }

    m.aae /= flows.size();
    m.are /= flows.size();
    m.are_hh = real_heavy_count == 0 ? 0.0 : m.are_hh / real_heavy_count;

    const double precision_den = static_cast<double>(true_positive + false_positive);
    const double recall_den = static_cast<double>(true_positive + false_negative);
    const double precision = precision_den == 0.0 ? 0.0 : true_positive / precision_den;
    const double recall = recall_den == 0.0 ? 0.0 : true_positive / recall_den;
    m.precision = precision;
    m.recall = recall;
    m.true_positive = true_positive;
    m.false_positive = false_positive;
    m.false_negative = false_negative;
    m.f1 = (precision + recall) == 0.0 ? 0.0 : 2.0 * precision * recall / (precision + recall);
    m.f1_hh = m.f1;
    const uint64_t true_negative = flows.size() - true_positive - false_positive - false_negative;
    m.accuracy = static_cast<double>(true_positive + true_negative) / flows.size();

    double wmre_num = 0.0;
    double wmre_den = 0.0;
    for (const auto& entry : real_hist) {
        const int size = entry.first;
        const int real_count = entry.second;
        const int est_count = est_hist.count(size) ? est_hist[size] : 0;
        wmre_num += std::abs(real_count - est_count);
        wmre_den += (std::abs(real_count) + std::abs(est_count)) / 2.0;
    }
    for (const auto& entry : est_hist) {
        if (real_hist.count(entry.first)) continue;
        wmre_num += std::abs(entry.second);
        wmre_den += std::abs(entry.second) / 2.0;
    }
    m.wmre = wmre_den == 0.0 ? 0.0 : wmre_num / wmre_den;

    const double real_entropy = entropy_from_hist(real_hist, real_packets);
    const double est_entropy = entropy_from_hist(est_hist, est_packets);
    m.re = real_entropy == 0.0 ? 0.0 : std::abs(est_entropy - real_entropy) / real_entropy;

    const std::chrono::duration<double> insert_seconds = insert_end - insert_begin;
    const std::chrono::duration<double> query_seconds = query_end - query_begin;
    m.insert_mpps = items.size() / insert_seconds.count() / 1e6;
    m.query_mqps = flows.size() / query_seconds.count() / 1e6;

    std::unique_ptr<Sketch> first_sketch(Choose_Sketch(constructor_memory_arg, d, seed, method.id));
    std::unique_ptr<Sketch> second_sketch(Choose_Sketch(constructor_memory_arg, d, seed, method.id));
    if (!first_sketch || !second_sketch) throw std::runtime_error("unknown sketch id");
    const auto first_candidates = insert_window_and_record_candidates(*first_sketch, first_items, change_threshold);
    const auto second_candidates = insert_window_and_record_candidates(*second_sketch, second_items, change_threshold);

    std::unordered_set<std::string> real_heavy_changes;
    std::unordered_set<std::string> estimated_heavy_changes;
    real_heavy_changes.reserve(change_flows.size());
    estimated_heavy_changes.reserve(first_candidates.size() + second_candidates.size());

    for (const auto& flow : change_flows) {
        if (flow.real >= change_threshold) real_heavy_changes.insert(flow.key);
    }
    auto check_change_candidate = [&](const std::string& key) {
        const int est_first = std::max(0, first_sketch->Query(key.c_str()));
        const int est_second = std::max(0, second_sketch->Query(key.c_str()));
        if (std::abs(est_second - est_first) >= change_threshold) {
            estimated_heavy_changes.insert(key);
        }
    };
    for (const auto& key : first_candidates) check_change_candidate(key);
    for (const auto& key : second_candidates) check_change_candidate(key);

    uint64_t hc_true_positive = 0, hc_false_positive = 0, hc_false_negative = 0;
    for (const auto& key : estimated_heavy_changes) {
        if (real_heavy_changes.count(key)) ++hc_true_positive;
        else ++hc_false_positive;
    }
    for (const auto& key : real_heavy_changes) {
        if (!estimated_heavy_changes.count(key)) ++hc_false_negative;
    }
    const double hc_precision_den = static_cast<double>(hc_true_positive + hc_false_positive);
    const double hc_recall_den = static_cast<double>(hc_true_positive + hc_false_negative);
    m.hc_precision = hc_precision_den == 0.0 ? 0.0 : hc_true_positive / hc_precision_den;
    m.hc_recall = hc_recall_den == 0.0 ? 0.0 : hc_true_positive / hc_recall_den;
    m.hc_true_positive = hc_true_positive;
    m.hc_false_positive = hc_false_positive;
    m.hc_false_negative = hc_false_negative;
    m.f1_hc = (m.hc_precision + m.hc_recall) == 0.0 ? 0.0 : 2.0 * m.hc_precision * m.hc_recall / (m.hc_precision + m.hc_recall);
    return m;
}

static void write_csv(const std::filesystem::path& path, const std::vector<Metrics>& rows) {
    std::ofstream out(path);
    out << "method,target_memory_bytes,target_memory_mb,actual_memory_bytes,actual_memory_mb,AAE,ARE,ARE_HH,F1,Accuracy,F1_HH,HH_precision,HH_recall,HH_TP,HH_FP,HH_FN,F1_HC,HC_precision,HC_recall,HC_TP,HC_FP,HC_FN,RE,WMRE,insert_Mpps,query_Mqps,query_CU_minimum,query_residual_minimum,query_residual_nonpositive_fallback\n";
    out << std::setprecision(10);
    for (const auto& m : rows) {
        out << m.method << ','
            << m.target_memory_bytes << ','
            << static_cast<double>(m.target_memory_bytes) / (1 << 20) << ','
            << m.actual_memory_bytes << ','
            << static_cast<double>(m.actual_memory_bytes) / (1 << 20) << ','
            << m.aae << ','
            << m.are << ','
            << m.are_hh << ','
            << m.f1 << ','
            << m.accuracy << ','
            << m.f1_hh << ','
            << m.precision << ','
            << m.recall << ','
            << m.true_positive << ','
            << m.false_positive << ','
            << m.false_negative << ','
            << m.f1_hc << ','
            << m.hc_precision << ','
            << m.hc_recall << ','
            << m.hc_true_positive << ','
            << m.hc_false_positive << ','
            << m.hc_false_negative << ','
            << m.re << ','
            << m.wmre << ','
            << m.insert_mpps << ','
            << m.query_mqps << ','
            << m.query_cu_minimum << ','
            << m.query_residual_minimum << ','
            << m.query_residual_nonpositive_fallback << '\n';
    }
}

static std::vector<Method> all_methods() {
    return {
        {"CM", 10},
        {"CM-CU", 20},
        {"CountLess", 40},
        {"Stingy", 0},
        {"Tailored", 51},
        {"o-Tailored", 50},
        {"Residual Sketch", 60},
        {"CU-only", 63},
        {"Residual-only", 64},
        {"Stable-Sketch", 61},
        {"LightGuardian", 62},
        {"Elastic", 70},
        {"UnivMon", 71},
        {"Nitro", 72},
        {"SketchVisor", 73},
    };
}

static std::vector<Method> parse_methods(const std::string& names) {
    if (names.empty() || names == "all") return all_methods();
    const std::vector<Method> known = all_methods();
    std::vector<Method> selected;
    std::stringstream ss(names);
    std::string name;
    while (std::getline(ss, name, ',')) {
        if (name == "KK-Sketch") name = "Residual Sketch";
        auto it = std::find_if(known.begin(), known.end(), [&](const Method& m) {
            return m.name == name;
        });
        if (it == known.end()) throw std::runtime_error("unknown method: " + name);
        selected.push_back(*it);
    }
    return selected;
}

int main(int argc, char** argv) {
    std::string dataset = "Data/zipf_example.dat";
    std::string change_dataset;
    int record_len = 10;
    int start_bytes = static_cast<int>(0.2 * (1 << 20));
    int end_bytes = 1 << 20;
    int step_bytes = static_cast<int>(0.1 * (1 << 20));
    int d = 3;
    int seed = 2026;
    int heavy_threshold = 0;
    int change_threshold = 0;
    std::string method_names = "all";
    std::filesystem::path out_dir = "../results/kk_sketch_zipf";

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto need_value = [&](const std::string& name) -> std::string {
            if (i + 1 >= argc) throw std::runtime_error("missing value for " + name);
            return argv[++i];
        };
        if (arg == "--dataset") dataset = need_value(arg);
        else if (arg == "--dataset2" || arg == "--change-dataset") change_dataset = need_value(arg);
        else if (arg == "--record-len") record_len = std::stoi(need_value(arg));
        else if (arg == "--start-bytes") start_bytes = std::stoi(need_value(arg));
        else if (arg == "--end-bytes") end_bytes = std::stoi(need_value(arg));
        else if (arg == "--step-bytes") step_bytes = std::stoi(need_value(arg));
        else if (arg == "--d") d = std::stoi(need_value(arg));
        else if (arg == "--seed") seed = std::stoi(need_value(arg));
        else if (arg == "--heavy-threshold") heavy_threshold = std::stoi(need_value(arg));
        else if (arg == "--change-threshold") change_threshold = std::stoi(need_value(arg));
        else if (arg == "--methods") method_names = need_value(arg);
        else if (arg == "--out-dir") out_dir = need_value(arg);
        else {
            std::cerr << "Unknown argument: " << arg << '\n';
            return 2;
        }
    }

    std::vector<std::string> items;
    std::vector<Flow> flows;
    if (!load_dataset(dataset, record_len, items, flows)) {
        std::cerr << "Failed to read dataset: " << dataset << '\n';
        return 1;
    }
    if (heavy_threshold <= 0) heavy_threshold = std::max(1, static_cast<int>(items.size() * 0.0001));

    std::vector<std::string> first_items;
    std::vector<std::string> second_items;
    std::vector<Flow> change_flows;
    int total_true_change = 0;
    if (change_dataset.empty()) {
        split_windows(items, first_items, second_items, change_flows, total_true_change);
    } else {
        std::vector<Flow> second_flows;
        first_items = items;
        if (!load_dataset(change_dataset, record_len, second_items, second_flows)) {
            std::cerr << "Failed to read second epoch dataset: " << change_dataset << '\n';
            return 1;
        }
        build_change_flows(first_items, second_items, change_flows, total_true_change);
    }
    if (change_threshold <= 0) {
        change_threshold = std::max(1, static_cast<int>(total_true_change * 0.0001));
    }

    std::filesystem::create_directories(out_dir);

    const std::vector<Method> methods = parse_methods(method_names);

    std::vector<Metrics> rows;
    std::cout << "Dataset: " << dataset << " items=" << items.size()
              << " flows=" << flows.size()
              << " change_dataset=" << (change_dataset.empty() ? "<split-main-dataset>" : change_dataset)
              << " heavy_threshold=" << heavy_threshold
              << " change_threshold=" << change_threshold
              << " total_true_change=" << total_true_change << '\n';

    for (int memory = start_bytes; memory <= end_bytes; memory += step_bytes) {
        for (const auto& method : methods) {
            const MemoryConfig mem = normalize_memory_budget(memory, d, method.id);
            std::cout << "Running " << method.name
                      << " target_memory=" << memory
                      << " actual_memory=" << mem.actual_bytes
                      << " constructor_arg=" << mem.constructor_arg
                      << " bytes" << std::endl;
            rows.push_back(evaluate(method,
                                    memory,
                                    mem.constructor_arg,
                                    mem.actual_bytes,
                                    d,
                                    seed,
                                    heavy_threshold,
                                    change_threshold,
                                    items,
                                    flows,
                                    first_items,
                                    second_items,
                                    change_flows));
            const Metrics& m = rows.back();
            std::cout << "  AAE=" << m.aae << " ARE=" << m.are
                      << " ARE_HH=" << m.are_hh
                      << " F1_HH=" << m.f1_hh << " Accuracy=" << m.accuracy << " F1_HC=" << m.f1_hc
                      << " RE=" << m.re << " WMRE=" << m.wmre
                      << " insert=" << m.insert_mpps << " Mpps"
                      << " query=" << m.query_mqps << " Mqps";
            if (method.id == 60) {
                std::cout << " query_routes(CU/residual/fallback)="
                          << m.query_cu_minimum << '/' << m.query_residual_minimum
                          << '/' << m.query_residual_nonpositive_fallback;
            }
            std::cout << '\n';
        }
    }

    const auto csv_path = out_dir / "kk_sketch_comparison.csv";
    write_csv(csv_path, rows);
    std::cout << "Wrote " << csv_path << '\n';
    return 0;
}
