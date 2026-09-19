#!/usr/bin/env bash
set -euo pipefail

output=${1:-/dev/stdout}

{
  echo "collected_at=$(date --iso-8601=seconds)"
  echo "hostname=$(hostname)"
  echo "kernel=$(uname -srvmo)"
  echo "compiler=$(cc --version | head -n 1)"
  echo "python=$(python3 --version)"
  echo "allowed_cpus=$(awk '/Cpus_allowed_list/ {print $2}' /proc/self/status)"
  echo
  echo "[lscpu]"
  lscpu
  echo
  echo "[governor]"
  for governor in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    if [[ -r "$governor" ]]; then
      echo "$governor=$(<"$governor")"
    fi
  done
  echo
  echo "[vulnerabilities]"
  for vulnerability in /sys/devices/system/cpu/vulnerabilities/*; do
    if [[ -r "$vulnerability" ]]; then
      echo "$(basename "$vulnerability")=$(<"$vulnerability")"
    fi
  done
} > "$output"
