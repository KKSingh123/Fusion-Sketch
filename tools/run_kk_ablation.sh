#!/usr/bin/env bash
set -euo pipefail

# Runs the CAIDA KK component ablation without overwriting the original comparison.
cmake -S src -B build
cmake --build build -j2

./build/KKSketch_Benchmark \
  --dataset src/Data/3.dat \
  --record-len 13 \
  --start-bytes 209715 \
  --end-bytes 1048576 \
  --step-bytes 104857 \
  --methods "Residual Sketch,CU-only,Residual-only" \
  --out-dir results/caida_ablation

env MPLBACKEND=Agg MPLCONFIGDIR=/tmp/mplconfig python3 tools/plot_kk_results.py \
  --csv results/caida_ablation/kk_sketch_comparison.csv \
  --out-dir results/caida_ablation/plots
