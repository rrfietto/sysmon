#!/usr/bin/env python3
"""Run each benchmark in isolation; no machine-dependent performance assertions."""
import argparse
import json
import math
from pathlib import Path
import platform
import subprocess


STAGES = ('host', 'memory', 'cpu', 'processes', 'sort-cpu', 'sort-memory',
          'sort-pid', 'render', 'refresh')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--iterations', type=int, default=100)
    parser.add_argument('--stage', choices=STAGES, action='append',
                        help='Run only these stages (repeatable)')
    parser.add_argument('--json', action='store_true', help='Emit machine-readable results')
    args = parser.parse_args()
    if not 1 <= args.iterations <= 100000:
        parser.error('--iterations must be between 1 and 100000')
    binary = Path(__file__).resolve().parent / 'benchmark'
    results = []
    for stage in args.stage or STAGES:
        completed = subprocess.run(
            [str(binary), stage, str(args.iterations)], stdin=subprocess.DEVNULL,
            capture_output=True, text=True, check=True, timeout=120)
        result = json.loads(completed.stdout)
        for key in ('mean_us', 'p50_us', 'p95_us', 'max_us', 'cpu_us'):
            if not math.isfinite(result[key]) or result[key] < 0:
                raise RuntimeError(f'Invalid measurement: {stage} {key}')
        if not result['p50_us'] <= result['p95_us'] <= result['max_us']:
            raise RuntimeError(f'Invalid latency percentiles: {stage}')
        results.append(result)
    if args.json:
        print(json.dumps({'platform': platform.platform(), 'results': results}, indent=2))
        return
    print(f'{platform.platform()} | {args.iterations} calls/stage | 5 warm-up calls')
    print(f'{"Stage":<14} {"Mean us":>11} {"P50 us":>11} {"P95 us":>11} '
          f'{"Max us":>11} {"CPU us/call":>12} {"Peak KiB":>10}')
    for result in results:
        print(f'{result["stage"]:<14} {result["mean_us"]:>11.2f} '
              f'{result["p50_us"]:>11.2f} {result["p95_us"]:>11.2f} '
              f'{result["max_us"]:>11.2f} {result["cpu_us"]:>12.2f} '
              f'{result["peak_rss_kib"]:>10}')
    print('\nIsolated stages ranked by mean CPU time (refresh is the combined path):')
    isolated = sorted((r for r in results if r['stage'] != 'refresh'),
                      key=lambda r: r['cpu_us'], reverse=True)
    for result in isolated:
        print(f'  {result["stage"]}: {result["cpu_us"]:.2f} us/call')
    for result in results:
        if result['stage'] in ('processes', 'refresh'):
            print(f'{result["stage"]}: mean {result["mean_processes"]:.1f} processes scanned')
        if result['stage'] in ('render', 'refresh'):
            print(f'{result["stage"]}: {result["bytes_per_frame"]:.0f} output bytes/frame')
    print('Peak RSS includes initialization and allocator caches; it is not allocation cost/call.')


if __name__ == '__main__':
    main()
