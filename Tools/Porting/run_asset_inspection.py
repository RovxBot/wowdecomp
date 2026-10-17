"""Run a hosted-built inspector locally; this tool never configures or compiles."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ['artifact', 'data', 'settings', 'cache', 'output']:
        parser.add_argument('--' + argument, type=Path, required=True)
    parser.add_argument('--locale', default='enUS')
    parser.add_argument('--sanitizers', action='store_true')
    args = parser.parse_args()
    manifest = json.loads((args.artifact/'manifest.json').read_text())
    name = 'NativeAssetCheck.exe' if platform.system() == 'Windows' else 'NativeAssetCheck'
    binary = args.artifact/'bin'/name
    actual = hashlib.sha256(binary.read_bytes()).hexdigest()
    expected = next(item['sha256'] for item in manifest['binaries'] if item['name'] == 'NativeAssetCheck')
    if actual != expected:
        raise SystemExit('Hosted inspector hash does not match its manifest')
    if platform.system() != 'Windows':
        binary.chmod(0o755)
    environment = os.environ.copy()
    if args.sanitizers:
        environment.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
    result = subprocess.run([str(binary.resolve()), str(args.data), args.locale, str(args.settings), str(args.cache)],
                            capture_output=True, text=True, timeout=60, env=environment)
    print(result.stdout, end='')
    print(result.stderr, end='')
    record = dict(scope='Native asset inspection of external original-data candidate; no rendering/gameplay',
                  executed_at=datetime.now(timezone.utc).isoformat(),
                  source_commit=manifest['source_commit'], hosted_build_run=manifest['workflow_run'],
                  binary_sha256=actual, os=platform.platform(), machine=platform.machine(),
                  locale=args.locale, data_root='external read-only reference',
                  sanitizer_options={key: environment.get(key) for key in ['ASAN_OPTIONS', 'UBSAN_OPTIONS']},
                  exit_code=result.returncode, stdout=result.stdout.splitlines(), stderr=result.stderr.splitlines())
    args.output.write_text(json.dumps(record, indent=2)+'\n')
    raise SystemExit(result.returncode)


if __name__ == '__main__':
    main()
