#!/usr/bin/env python3
"""Run correctness checks first, then paired socket / student-SHM measurements."""
import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import signal
import statistics
import subprocess
import sys
import time

LAB = Path(__file__).resolve().parents[1]
SIZES = (1, 64, 1024, 4096)


def run_timed(command, timeout):
    """Kill the complete benchmark process group on timeout, including its server."""
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               text=True, start_new_session=True)
    try:
        out, err = process.communicate(timeout=timeout)
        return process.returncode, out, err
    except subprocess.TimeoutExpired:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        out, err = process.communicate()
        return 124, out, err + f"\nTIMEOUT after {timeout}s; client and server stopped\n"
    except BaseException:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        process.communicate()
        raise


def quantile(values, p):
    values = sorted(values)
    at = (len(values) - 1) * p
    lo, hi = math.floor(at), math.ceil(at)
    return values[lo] + (values[hi] - values[lo]) * (at - lo)


def summarize(paths, output):
    rows = []
    for path in paths:
        with path.open() as source:
            records = list(csv.DictReader(source))
        if not records:
            raise ValueError(f"no data in {path}")
        head = records[0]
        values = [float(r['ns_per_op']) for r in records]
        rows.append({**{k: head[k] for k in ('transport', 'placement', 'client_cpu', 'server_cpu',
                                          'message_bytes', 'packet_bytes')},
                     'samples': len(values), 'median_batch_mean_ns_per_rtt': statistics.median(values),
                     'mean_batch_mean_ns_per_rtt': statistics.fmean(values),
                     'p95_batch_mean_ns_per_rtt': quantile(values, .95)})
    with (output / 'summary.csv').open('w') as destination:
        writer = csv.DictWriter(destination, fieldnames=list(rows[0]))
        writer.writeheader(); writer.writerows(rows)
    groups = {(r['transport'], r['placement'], r['message_bytes']): r for r in rows}
    paired = []
    for row in rows:
        key = ('socket', row['placement'], row['message_bytes'])
        if row['transport'] != 'shm' or key not in groups:
            continue
        socket = groups[key]['median_batch_mean_ns_per_rtt']
        shm = row['median_batch_mean_ns_per_rtt']
        paired.append({'placement': row['placement'], 'message_bytes': row['message_bytes'],
                       'socket_median_ns': socket, 'shm_median_ns': shm,
                       'shm_minus_socket_ns': shm - socket,
                       'shm_over_socket_ratio': shm / socket if socket else ''})
    if paired:
        with (output / 'comparison.csv').open('w') as destination:
            writer = csv.DictWriter(destination, fieldnames=list(paired[0]))
            writer.writeheader(); writer.writerows(paired)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=('check', 'bench'), default='check')
    parser.add_argument('--transport', choices=('socket', 'shm', 'both'), default='both')
    parser.add_argument('--placements', nargs='+', choices=('same', 'different'), default=['same', 'different'])
    parser.add_argument('--client-cpu', type=int)
    parser.add_argument('--server-cpu', type=int, help='server CPU for different placement')
    parser.add_argument('--check-iterations', type=int, default=5000)
    parser.add_argument('--samples', type=int, default=51)
    parser.add_argument('--iterations', type=int, default=1000)
    parser.add_argument('--seeds', nargs='+', type=int, default=[1, 17, 2026])
    parser.add_argument('--timeout', type=float, default=30)
    parser.add_argument('--output', type=Path, help='new directory only; existing results are never overwritten')
    args = parser.parse_args()
    if min(args.check_iterations, args.samples, args.iterations) < 1 or args.timeout <= 0:
        parser.error('iterations, samples and timeout must be positive')
    if max(args.check_iterations, args.samples, args.iterations) > 2**32-1:
        parser.error('iterations and samples must fit unsigned 32-bit integers')
    if any(s < 0 or s > 2**32-1 for s in args.seeds):
        parser.error('seed must fit an unsigned 32-bit integer')
    allowed = sorted(os.sched_getaffinity(0))
    client = args.client_cpu if args.client_cpu is not None else allowed[0]
    others = [cpu for cpu in allowed if cpu != client]
    different = args.server_cpu if args.server_cpu is not None else (others[0] if others else None)
    if client not in allowed:
        parser.error('client CPU is not allowed')
    if 'different' in args.placements and (different not in allowed or different == client):
        parser.error('different placement requires another allowed CPU; use --placements same on a one-CPU machine')
    output = args.output or LAB / 'results' / f'advanced-{args.mode}-{time.time_ns()}'
    if output.exists():
        parser.error(f'output already exists: {output}; choose a new directory')
    output.mkdir(parents=True)
    subprocess.run(['make', '-C', str(LAB / 'linux'), 'advanced'], check=True)
    binary = LAB / 'linux/bin/bench_ipc_compare'
    subprocess.run([str(LAB / 'scripts/collect-env.sh'), str(output / 'environment.txt')], check=True)
    source_paths = [*sorted((LAB / 'linux/src/advanced').glob('*')), LAB / 'linux/Makefile', Path(__file__), binary]
    metadata = {'arguments': {k: str(v) if isinstance(v, Path) else v for k,v in vars(args).items()},
                'allowed_cpus': allowed, 'client_cpu': client, 'different_server_cpu': different,
                'source_sha256': {str(p.relative_to(LAB)): hashlib.sha256(p.read_bytes()).hexdigest() for p in source_paths},
                'runs': [], 'status': 'running'}
    for rel in ['linux/src/advanced', 'linux/Makefile', 'scripts/run-advanced.py']:
        source = LAB / rel
        files = source.glob('*') if source.is_dir() else [source]
        for item in files:
            target = output / 'source' / item.relative_to(LAB)
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(item.read_bytes())
    manifest = output / 'run.json'
    def save():
        manifest.write_text(json.dumps(metadata, indent=2) + '\n')
    save()
    transports = ['socket', 'shm'] if args.transport == 'both' else [args.transport]
    placements = list(dict.fromkeys(args.placements))
    configs = [(t, p, size) for p in placements for size in SIZES for t in transports]
    data = []
    def execute(mode, transport, placement, size, seed):
        stem = f'{mode}-{placement}-{transport}-{size}-seed{seed}'
        command = [str(binary), '--transport', transport, '--mode', mode,
                   '--client-cpu', str(client), '--server-cpu', str(client if placement == 'same' else different),
                   '--message-bytes', str(size), '--seed', str(seed), '--warmup', '50',
                   '--samples', str(1 if mode == 'check' else args.samples),
                   '--iterations', str(args.check_iterations if mode == 'check' else args.iterations)]
        code, stdout, stderr = run_timed(command, args.timeout)
        path = output / (stem + ('.csv' if mode == 'bench' else '.stdout.txt'))
        path.write_text(stdout)
        (output / (stem + '.stderr.txt')).write_text(stderr)
        metadata['runs'].append({'command': command, 'returncode': code, 'stdout': path.name,
                                 'stderr': stem + '.stderr.txt'})
        if code or (mode == 'check' and 'CHECK PASS' not in stdout):
            metadata['status'] = 'failed'; save()
            print(f'FAILED: {stem}; see {output}. No performance summary was produced.', file=sys.stderr)
            return False
        if mode == 'bench':
            records = list(csv.DictReader(stdout.splitlines()))
            if len(records) != args.samples:
                metadata['status'] = 'failed'; save()
                raise ValueError(f'incorrect sample count: {stem}')
            data.append(path)
        save()
        return True
    # Complete every correctness configuration before starting any timings.
    for transport, placement, size in configs:
        for seed in args.seeds:
            if not execute('check', transport, placement, size, seed):
                return 1
    if args.mode == 'bench':
        # One configuration at a time, serially. Repeat the entire run if order effects matter.
        for transport, placement, size in configs:
            if not execute('bench', transport, placement, size, args.seeds[0]):
                return 1
        summarize(data, output)
    metadata['status'] = 'passed'; save()
    print(f'{args.mode.upper()} PASS: {output}')
    return 0

if __name__ == '__main__':
    sys.exit(main())
