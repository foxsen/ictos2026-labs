"""Checks the supplied harness, not an answer to the student's SHM task."""
import csv
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

LAB = Path(__file__).resolve().parents[1]
SRC = LAB / 'linux/src/advanced'
spec = importlib.util.spec_from_file_location('advanced_runner', LAB / 'scripts/run-advanced.py')
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)

class HarnessTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.directory = Path(cls.temp.name)
        cls.allowed = sorted(os.sched_getaffinity(0))
        cls.binaries = {}
        # Independent error fixture: tests remain valid after students implement SHM.
        error_source = cls.directory / 'error_transport.c'
        error_source.write_text('#include "transport.h"\n#include <errno.h>\n#include <stdio.h>\n'
                                'static int fail(ipc_transport_t *t) {(void)t; '
                                'fputs("TEST TRANSPORT ERROR\\n",stderr); errno=ENOSYS; return -1;}\n'
                                'const ipc_ops_t shm_ops={.name="shm",.init=fail};\n')
        original = (SRC / 'bench_compare.c').read_text()
        variants = {
            'normal': original,
            'corrupt': original.replace('memcpy(reply, request, packet_bytes);',
                                       'memcpy(reply, request, packet_bytes); reply[IPC_SEQUENCE_BYTES] ^= 1;'),
            'stale': original.replace('memcpy(reply, request, packet_bytes);',
                                     'memcpy(reply, request, packet_bytes); memset(reply, 0, IPC_SEQUENCE_BYTES);'),
        }
        for name, text in variants.items():
            source = cls.directory / f'{name}.c'; source.write_text(text)
            binary = cls.directory / name
            subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O2', '-Wall', '-Wextra',
                            '-Werror', '-I', str(SRC), str(source), str(SRC / 'transport_socket.c'),
                            str(error_source), '-o', str(binary)], check=True)
            cls.binaries[name] = binary

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def command(self, variant='normal', transport='socket', mode='check', size=64, server=None):
        return [str(self.binaries[variant]), '--transport', transport, '--mode', mode,
                '--client-cpu', str(self.allowed[0]), '--server-cpu', str(self.allowed[0] if server is None else server),
                '--message-bytes', str(size), '--samples', '3', '--iterations', '80',
                '--seed', '17', '--warmup', '5']

    def test_socket_correctness_payloads_and_placements(self):
        for server in self.allowed[:2]:
            for size in (1, 7, 64, 1024, 4096):
                with self.subTest(server=server, size=size):
                    code, out, err = runner.run_timed(self.command(size=size, server=server), 5)
                    self.assertEqual(code, 0, err)
                    self.assertIn('round_trips=245', out)

    def test_benchmark_csv_and_summary(self):
        code, out, err = runner.run_timed(self.command(mode='bench'), 5)
        self.assertEqual(code, 0, err)
        rows = list(csv.DictReader(out.splitlines()))
        self.assertEqual(len(rows), 3)
        for row in rows:
            self.assertEqual(row['packet_bytes'], '72')
            self.assertEqual(row['iterations'], '80')
            self.assertGreater(float(row['ns_per_op']), 0)
        path = self.directory / 'data.csv'; path.write_text(out)
        runner.summarize([path], self.directory)
        with (self.directory / 'summary.csv').open() as f:
            summary = list(csv.DictReader(f))
        self.assertEqual(summary[0]['samples'], '3')
        self.assertFalse((self.directory / 'comparison.csv').exists())

    def test_transport_error_propagated(self):
        code, out, err = runner.run_timed(self.command(transport='shm'), 5)
        self.assertNotEqual(code, 0)
        self.assertIn('TEST TRANSPORT ERROR', err)
        self.assertNotIn('CHECK PASS', out)

    def test_payload_corruption_detected(self):
        code, out, err = runner.run_timed(self.command(variant='corrupt'), 5)
        self.assertNotEqual(code, 0)
        self.assertIn('payload mismatch', err)
        self.assertNotIn('CHECK PASS', out)

    def test_stale_reply_detected(self):
        code, out, err = runner.run_timed(self.command(variant='stale'), 5)
        self.assertNotEqual(code, 0)
        self.assertIn('sequence mismatch', err)
        self.assertNotIn('CHECK PASS', out)

    def test_timeout_stops_process_group(self):
        command = [sys.executable, '-c',
                   'import os,time; p=os.fork(); print(p,flush=True) if p else None; time.sleep(60)']
        code, out, err = runner.run_timed(command, .3)
        self.assertEqual(code, 124)
        self.assertIn('TIMEOUT', err)
        child = int(out.strip())
        stat = Path(f'/proc/{child}/stat')
        try:
            state = stat.read_text().split(') ')[1].split()[0]
        except (FileNotFoundError, ProcessLookupError):
            return  # The kernel may reap the child between lookup and read.
        self.assertEqual(state, 'Z')

if __name__ == '__main__':
    unittest.main()
