#!/usr/bin/env bash
set -euo pipefail

cmake -S src -B build
cmake --build build -j2

./build/KKSketch_Benchmark \
  --dataset src/Data/zipf_example.dat \
  --record-len 10 \
  --start-bytes 209715 \
  --end-bytes 1048576 \
  --step-bytes 209715 \
  --out-dir results/kk_sketch_zipf

python3 tools/plot_kk_results.py \
  --csv results/kk_sketch_zipf/kk_sketch_comparison.csv \
  --out-dir results/kk_sketch_zipf/plots
