#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
lab_dir=$(cd -- "$script_dir/.." && pwd)

docker_args=(
  run --rm -it
  --user "$(id -u):$(id -g)"
  --env HOME=/tmp/student-home
  --volume "$lab_dir:/lab"
  --workdir /lab
)

if [[ -e /dev/kvm ]]; then
  docker_args+=(--device /dev/kvm)
fi

docker "${docker_args[@]}" ictos-kernel-boundary:2026 \
  bash -lc 'mkdir -p "$HOME" && exec bash'
