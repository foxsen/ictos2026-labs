#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
profile=${1:-tcg}

"$script_dir/setup.sh"
for fastpath in on off; do
  "$script_dir/build.sh" "$profile" "$fastpath"
  "$script_dir/run.sh" "$profile" "$fastpath"
done
