#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
lab_dir=$(cd -- "$script_dir/.." && pwd)
output_dir=${1:-"$lab_dir/results/linux"}
allowed_cpus=$(awk '/Cpus_allowed_list/ {print $2}' /proc/self/status)
first_range=${allowed_cpus%%,*}
default_cpu=${first_range%%-*}
cpu=${CPU:-$default_cpu}
syscall_samples=${SYSCALL_SAMPLES:-101}
syscall_iterations=${SYSCALL_ITERATIONS:-10000}
ipc_samples=${IPC_SAMPLES:-51}
ipc_iterations=${IPC_ITERATIONS:-1000}

mkdir -p "$output_dir"
make -C "$lab_dir/linux" all

"$script_dir/collect-env.sh" "$output_dir/environment.txt"

"$lab_dir/linux/bin/bench_syscall" \
  --cpu "$cpu" \
  --samples "$syscall_samples" \
  --iterations "$syscall_iterations" \
  > "$output_dir/syscall.csv" \
  2> "$output_dir/syscall.stderr.txt"

"$lab_dir/linux/bin/bench_ipc" \
  --cpu "$cpu" \
  --samples "$ipc_samples" \
  --iterations "$ipc_iterations" \
  > "$output_dir/ipc.csv" \
  2> "$output_dir/ipc.stderr.txt"

python3 "$script_dir/summarize.py" \
  "$output_dir/syscall.csv" \
  "$output_dir/ipc.csv" \
  --output "$output_dir/summary.csv"

echo "Linux results written to $output_dir"
echo "Pinned CPU: $cpu (allowed: $allowed_cpus)"
