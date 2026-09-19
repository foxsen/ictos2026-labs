#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
lab_dir=$(cd -- "$script_dir/.." && pwd)
work_dir=${SEL4_WORK_DIR:-"$lab_dir/.work/sel4bench"}
profile=${1:-tcg}
fastpath=${2:-on}
output_dir=${3:-"$lab_dir/results/sel4-${profile}-fastpath-${fastpath}"}
build_dir="$work_dir/build-${profile}-fastpath-${fastpath}"

if [[ "$profile" != "tcg" && "$profile" != "kvm" ]]; then
  echo "usage: $0 {tcg|kvm} {on|off} [output-directory]" >&2
  exit 2
fi
if [[ "$fastpath" != "on" && "$fastpath" != "off" ]]; then
  echo "usage: $0 {tcg|kvm} {on|off} [output-directory]" >&2
  exit 2
fi
if [[ ! -x "$build_dir/simulate" ]]; then
  echo "build not found: $build_dir" >&2
  echo "run: $script_dir/build.sh $profile $fastpath" >&2
  exit 1
fi

mkdir -p "$output_dir"
python3 "$lab_dir/scripts/run-sel4.py" \
  --build-dir "$build_dir" \
  --profile "$profile" \
  --log "$output_dir/serial.log" \
  --timeout "${SEL4_TIMEOUT:-180}"

python3 "$lab_dir/scripts/parse-sel4.py" \
  "$output_dir/serial.log" \
  --json "$output_dir/results.json" \
  --csv "$output_dir/ipc.csv" \
  --profile "$profile" \
  --fastpath "$fastpath"

cp "$work_dir/pinned-manifest.xml" "$output_dir/pinned-manifest.xml"
echo "seL4 results written to $output_dir"
