#!/usr/bin/env python3
"""Compile each unmodified production TU for iOS; never suppress layout guards."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--sdk', default='iphoneos', choices=['iphoneos', 'iphonesimulator'])
p.add_argument('--arch', default='arm64', choices=['arm64', 'x86_64'])
args = p.parse_args()
out = root / 'build' / ('ios-audit-' + args.arch)
out.mkdir(parents=True, exist_ok=True)
sdk = subprocess.check_output(['xcrun', '--sdk', args.sdk, '--show-sdk-path'], text=True).strip()
compiler = subprocess.check_output(['xcrun', '--sdk', args.sdk, '--find', 'clang++'], text=True).strip()
target = args.arch + '-apple-ios14.0' + ('-simulator' if args.sdk == 'iphonesimulator' else '')
base = [compiler, '-target', target, '-isysroot', sdk, '-std=c++17', '-fms-extensions',
        '-fsyntax-only', '-ferror-limit=0', '-Wno-ignored-attributes', '-Wno-writable-strings',
        '-I' + str(root / 'ios/compat/include'), '-I' + str(root / 'src'),
        '-include', str(root / 'ios/compat/compile_probe.hpp')]
results = []
for source in sorted((root / 'src').rglob('*.cpp')):
    command = base + [str(source)]
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    relative = source.relative_to(root).as_posix()
    log = relative.replace('/', '_') + '.log'
    (out / log).write_text(result.stdout)
    errors = [line for line in result.stdout.splitlines() if ': error:' in line or ': fatal error:' in line]
    results.append(dict(source=relative, sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                        command=command, returncode=result.returncode, errors=errors, log=log))
    print(relative, 'PASS' if result.returncode == 0 else f'BLOCKED ({len(errors)} errors)', flush=True)
report = dict(target=target, compiler=subprocess.check_output([compiler, '--version'], text=True),
              passed=sum(r['returncode'] == 0 for r in results), total=len(results), results=results)
(out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
print(f"{report['passed']}/{report['total']} production translation units compile unchanged")
raise SystemExit(0 if report['passed'] == report['total'] else 1)
