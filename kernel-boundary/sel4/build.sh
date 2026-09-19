#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
lab_dir=$(cd -- "$script_dir/.." && pwd)
work_dir=${SEL4_WORK_DIR:-"$lab_dir/.work/sel4bench"}
profile=${1:-tcg}
fastpath=${2:-on}

if [[ "$profile" != "tcg" && "$profile" != "kvm" ]]; then
  echo "usage: $0 {tcg|kvm} {on|off}" >&2
  exit 2
fi
if [[ "$fastpath" != "on" && "$fastpath" != "off" ]]; then
  echo "usage: $0 {tcg|kvm} {on|off}" >&2
  exit 2
fi
if [[ ! -x "$work_dir/.venv/bin/python" ]]; then
  echo "seL4 work tree is missing; run $script_dir/setup.sh first" >&2
  exit 1
fi

source "$work_dir/.venv/bin/activate"
build_dir="$work_dir/build-${profile}-fastpath-${fastpath}"
mkdir -p "$build_dir"

fastpath_value=OFF
if [[ "$fastpath" == "on" ]]; then
  fastpath_value=ON
fi

if [[ ! -f "$build_dir/CMakeCache.txt" ]]; then
  (
    cd "$build_dir"
    ../griddle \
      --PLATFORM=x86_64 \
      --FASTPATH="$fastpath_value" \
      --IPC=ON \
      --SCHED=OFF \
      --IRQUSER=OFF \
      --SIGNAL=OFF \
      --MAPPING=OFF \
      --SYNC=OFF
  )
fi

if [[ "$profile" == "tcg" ]]; then
  cmake \
    -S "$work_dir/projects/sel4bench" \
    -B "$build_dir" \
    -DKernelSupportPCID=OFF \
    -DKernelFSGSBase=msr \
    -DKernelHugePage=OFF
fi

ninja -C "$build_dir" -j"${JOBS:-4}"
echo "built $build_dir"
