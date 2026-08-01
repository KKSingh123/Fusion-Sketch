Dataset is example of Zipf dataset with skewness 1.0 located in  ./src/Data   

Different Sketch is in ./src/Sketch

Measuremnet task only has per-flow size estimation and the throughput.

The estimation result of each flow is writed in the file,

other tasks' (e.g., heavy hitter，heavy change, flow size distribution, entropy estimation) result can be getted by the per-flow size.   


















cmake --build build -j2
## Zif data set 
- ./build/KKSketch_Benchmark \
  --dataset src/Data/zipf_example.dat \
  --record-len 10 \
  --start-bytes 209715 \
  --end-bytes 1048576 \
  --step-bytes 104857 \
  --methods all \
  --out-dir results/kk_sketch_zipf

## Plot results
- env MPLBACKEND=Agg MPLCONFIGDIR=/tmp/mplconfig python3 tools/plot_kk_results.py \
  --csv results/kk_sketch_zipf/kk_sketch_comparison.csv \
  --out-dir results/kk_sketch_zipf/plots

## CAIDA KK component ablation (CU-only vs. residual-only)

This leaves `results/caida_results` unchanged and writes the full benchmark
metric set and plots to `results/caida_ablation` using `src/Data/3.dat`.

```bash
bash tools/run_kk_ablation.sh
```

## Zipf KK component ablation (CU-only vs. residual-only)

This writes the full benchmark metric set and plots to `results/zipf_ablation`.

```bash
bash tools/run_kk_zipf_ablation.sh
```



## Caida data set 
- ./build/KKSketch_Benchmark \
  --dataset src/Data/3.dat \
  --record-len 13 \
  --start-bytes 209715 \
  --end-bytes 1048576 \
  --step-bytes 104857 \
  --methods all \
  --out-dir results/caida_results

  # plot Results
  - env MPLBACKEND=Agg MPLCONFIGDIR=/tmp/mplconfig python3 tools/plot_kk_results.py \
  --csv results/caida_results/kk_sketch_comparison.csv \
  --out-dir results/caida_results/plots

For CAIDA, keep the same `--out-dir results/caida_results` for `2.dat`, `3.dat`, `4.dat`, or `5.dat`.
Change only `--dataset`; the CSV and plots in `results/caida_results` will be updated.
