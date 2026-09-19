#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
lab_dir=$(cd -- "$script_dir/.." && pwd)
work_dir=${SEL4_WORK_DIR:-"$lab_dir/.work/sel4bench"}
profile=${1:-tcg}
output=${2:-"$lab_dir/teacher/sel4-${profile}-prebuilt.tar.gz"}
staging=$(mktemp -d)
trap 'rm -rf -- "$staging"' EXIT

for fastpath in on off; do
  build_dir="$work_dir/build-${profile}-fastpath-${fastpath}"
  if [[ ! -x "$build_dir/simulate" ]]; then
    echo "missing build: $build_dir" >&2
    echo "run: $lab_dir/sel4/build.sh $profile $fastpath" >&2
    exit 1
  fi
  destination="$staging/build-${profile}-fastpath-${fastpath}"
  mkdir -p "$destination/images"
  cp "$build_dir/simulate" "$destination/"
  cp "$build_dir/images/kernel-x86_64-pc99" "$destination/images/"
  cp "$build_dir/images/sel4benchapp-image-x86_64-pc99" "$destination/images/"
done

cp "$work_dir/pinned-manifest.xml" "$staging/"
cp "$lab_dir/sel4/versions.env" "$staging/"
(
  cd "$staging"
  sha256sum \
    "build-${profile}-fastpath-on/images/"* \
    "build-${profile}-fastpath-off/images/"* \
    pinned-manifest.xml \
    versions.env \
    > SHA256SUMS
)
tar -C "$staging" -czf "$output" .
echo "prebuilt bundle written to $output"
