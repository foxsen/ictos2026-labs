#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
lab_dir=$(cd -- "$script_dir/.." && pwd)
source "$script_dir/versions.env"

work_dir=${SEL4_WORK_DIR:-"$lab_dir/.work/sel4bench"}

for command in git repo python3 cmake ninja qemu-system-x86_64 protoc; do
  if ! command -v "$command" >/dev/null 2>&1; then
    echo "missing required command: $command" >&2
    echo "use the supplied container or install the host dependencies listed in INSTRUCTOR.md" >&2
    exit 1
  fi
done

mkdir -p "$work_dir"
if [[ ! -d "$work_dir/.repo" ]]; then
  (
    cd "$work_dir"
    repo init \
      -u "$SEL4BENCH_MANIFEST_URL" \
      -b "refs/tags/$SEL4BENCH_TAG"
  )
fi

(
  cd "$work_dir"
  repo sync -c -j"${JOBS:-4}" --no-clone-bundle
)

actual_manifest_commit=$(git -C "$work_dir/.repo/manifests" rev-parse HEAD)
if [[ "$actual_manifest_commit" != "$SEL4BENCH_MANIFEST_COMMIT" ]]; then
  echo "unexpected manifest commit: $actual_manifest_commit" >&2
  echo "expected: $SEL4BENCH_MANIFEST_COMMIT" >&2
  exit 1
fi

if [[ ! -x "$work_dir/.venv/bin/python" ]]; then
  python3 -m venv "$work_dir/.venv"
fi
"$work_dir/.venv/bin/pip" install --disable-pip-version-check "$work_dir/kernel/tools/python-deps"

(
  cd "$work_dir"
  repo manifest -r -o "$work_dir/pinned-manifest.xml"
)
echo "seL4bench $SEL4BENCH_TAG is ready in $work_dir"
