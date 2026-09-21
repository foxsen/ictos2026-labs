#!/usr/bin/env bash
set -euo pipefail
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
lab_dir=$(cd -- "$script_dir/.." && pwd)
output=${1:-"$script_dir/kernel-boundary-starter.tar.gz"}
if [[ -e "$output" ]]; then
  echo "output exists; choose another filename: $output" >&2
  exit 1
fi
mkdir -p -- "$(dirname -- "$output")"
staging=$(mktemp -d)
trap 'rm -rf -- "$staging"' EXIT
bundle="$staging/kernel-boundary"
mkdir -p "$bundle/linux/src/advanced" "$bundle/scripts" "$bundle/sel4" "$bundle/environment" "$bundle/tests"
cp "$lab_dir/README.md" "$lab_dir/ADVANCED.md" "$lab_dir/report-template.md" "$lab_dir/.gitignore" "$bundle/"
cp "$lab_dir/linux/Makefile" "$bundle/linux/"
cp "$lab_dir/linux/src/"*.c "$bundle/linux/src/"
cp "$lab_dir/linux/src/advanced/"*.c "$lab_dir/linux/src/advanced/"*.h "$bundle/linux/src/advanced/"
cp "$lab_dir/scripts/"*.sh "$lab_dir/scripts/"*.py "$bundle/scripts/"
cp "$lab_dir/sel4/"*.sh "$lab_dir/sel4/versions.env" "$lab_dir/sel4/README.md" "$bundle/sel4/"
cp "$lab_dir/environment/Dockerfile" "$lab_dir/environment/"*.sh "$bundle/environment/"
cp "$lab_dir/tests/test_advanced.py" "$bundle/tests/"
python3 - "$bundle" "$lab_dir" <<'PY'
import hashlib, json, subprocess, sys
from datetime import datetime, timezone
from pathlib import Path
bundle, lab = map(Path, sys.argv[1:])
revision = subprocess.run(['git', '-C', str(lab), 'rev-parse', 'HEAD'], capture_output=True, text=True)
info = {'created_utc': datetime.now(timezone.utc).isoformat(),
        'base_revision': revision.stdout.strip() if revision.returncode == 0 else 'unavailable',
        'contents': 'Actual working files; SHA256SUMS identifies this package, including uncommitted changes.',
        'student_task': 'Implement transport_shm.c; no working shared-memory transport included.'}
(bundle / 'PACKAGE.json').write_text(json.dumps(info, indent=2) + '\n')
files = sorted(p for p in bundle.rglob('*') if p.is_file())
(bundle / 'SHA256SUMS').write_text(''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.relative_to(bundle)}\n' for p in files))
PY
tar -C "$staging" -czf "$output" kernel-boundary
printf 'student starter written to %s\n' "$output"
