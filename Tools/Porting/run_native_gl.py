"""Run a hash-verified Actions OpenGL probe; never compile locally."""
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
    parser.add_argument('--artifact', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--snapshot', type=Path)
    parser.add_argument('--images', action='store_true', help='Run image sources, atlases and mip Services checks (three cycles)')
    parser.add_argument('--frame-hierarchy', action='store_true', help='Run original Frame parents/layers/root (three cycles)')
    parser.add_argument('--frame-regions', action='store_true', help='Run original unparented FontString regions (three cycles)')
    parser.add_argument('--frame-render', action='store_true', help='Run retained original Frame render batches (three cycles)')
    parser.add_argument('--frame-fonts', action='store_true', help='Run original Frame font/name/XML/Lua factories (three cycles)')
    parser.add_argument('--legacy-gxu', action='store_true', help='Run original Gxu handles/layout/cache/batches (three cycles)')
    parser.add_argument('--font', type=Path, help='Authored cache font for synthetic Gxu checks')
    parser.add_argument('--legacy-device', action='store_true', help='Run the actual legacy Gx dispatch fixture (three cycles)')
    parser.add_argument('--frames', type=int, default=120)
    parser.add_argument('--cycles', type=int, default=1)
    parser.add_argument('--timeout', type=int, default=120)
    parser.add_argument('--sanitizers', action='store_true')
    parser.add_argument('--render-target-checks', action='store_true',
                        help='Also hash-verify and run the three-cycle native Gx target fixture')
    parser.add_argument('--transform-checks', action='store_true',
                        help='Also hash-verify and run the three-cycle native Gx transform fixture')
    parser.add_argument('--buffer-checks', action='store_true',
                        help='Also hash-verify and run native Gx buffer/vertex submission checks')
    parser.add_argument('--texture-checks', action='store_true',
                        help='Also verify and run native texture format/update/sampling checks')
    for name in ['data', 'settings', 'cache']:
        parser.add_argument('--' + name, type=Path)
    parser.add_argument('--locale', default='enUS')
    args = parser.parse_args()
    if platform.system() != 'Linux' or platform.machine() != 'x86_64':
        parser.error('This probe currently targets Linux x86-64')
    if not 1 <= args.frames <= 36000 or not 1 <= args.cycles <= 20 or args.timeout < 1:
        parser.error('Invalid frame/cycle count or timeout')
    if any([args.data, args.settings, args.cache]) and not all([args.data, args.settings, args.cache]):
        parser.error('Use --data, --settings and --cache together')
    if args.data:
        # Treat the entire installation as read-only, including captures/reports.
        installation = args.data.resolve().parent
        for output in [args.output, args.snapshot, args.settings, args.cache]:
            if output and output.resolve().is_relative_to(installation):
                parser.error('Outputs and user roots must be outside the game installation')
    args.output.unlink(missing_ok=True)
    manifest = json.loads((args.artifact / 'manifest.json').read_text())
    if manifest['preset'] not in {'linux-gl', 'linux-gl-asan'} or manifest['target_os'] != 'LINUX':
        parser.error('Expected a Linux M3 OpenGL Actions artifact')
    if sum([args.legacy_device, args.legacy_gxu, args.frame_fonts, args.frame_hierarchy, args.frame_regions, args.frame_render, args.images]) > 1:
        parser.error('Select one legacy fixture')
    legacy = args.legacy_device or args.legacy_gxu or args.frame_fonts or args.frame_hierarchy or args.frame_regions or args.frame_render or args.images
    probe_name = 'NativeFrameHierarchyCheck' if args.frame_hierarchy else 'NativeFrameRegionsCheck' if args.frame_regions else 'NativeFrameRenderCheck' if args.frame_render else 'NativeImageCheck' if args.images else 'NativeFrameFontsCheck' if args.frame_fonts else 'LegacyGxuCheck' if args.legacy_gxu else 'LegacyGxCheck' if args.legacy_device else 'NativeGlCheck'
    if args.legacy_device and args.snapshot:
        parser.error('The legacy fixture currently records pixels, without a snapshot')
    binary = args.artifact / 'bin' / probe_name
    actual = hashlib.sha256(binary.read_bytes()).hexdigest()
    expected = next(item['sha256'] for item in manifest['binaries'] if item['name'] == probe_name)
    if actual != expected:
        raise SystemExit('Hosted OpenGL binary hash does not match the manifest')
    binary.chmod(0o755)
    checks = []
    for enabled, name, key, label in [
            (args.render_target_checks, 'GlRenderTargetsCheck', 'render_target_checks', 'render-target'),
            (args.transform_checks, 'GlTransformsCheck', 'transform_checks', 'transform'),
            (args.buffer_checks, 'GlBuffersCheck', 'buffer_checks', 'buffer'),
            (args.texture_checks, 'GlTexturesCheck', 'texture_checks', 'texture')]:
        if not enabled:
            continue
        check_binary = args.artifact / 'bin' / name
        entry = next((item for item in manifest['binaries'] if item['name'] == name), None)
        if entry is None or not check_binary.is_file():
            raise SystemExit(f'Hosted artifact does not contain native {label} checks')
        check_hash = hashlib.sha256(check_binary.read_bytes()).hexdigest()
        if check_hash != entry['sha256']:
            raise SystemExit(f'Hosted {label} binary hash does not match the manifest')
        check_binary.chmod(0o755)
        checks.append((key, check_binary, check_hash))
    environment = os.environ.copy()
    environment.update(SDL_VIDEODRIVER='x11', EGL_PLATFORM='x11')
    if args.sanitizers or manifest['preset'].endswith('-asan'):
        environment.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
    command = [str(binary.resolve())]
    if not legacy:
        command += ['--frames', str(args.frames), '--cycles', str(args.cycles)]
    if args.font:
        if not (args.legacy_gxu or args.frame_fonts or args.frame_hierarchy or args.frame_regions or args.frame_render) or args.data:
            parser.error('--font is only for synthetic Gxu/Frame font checks')
        command += ['--font', str(args.font.resolve())]
    if args.snapshot:
        args.snapshot.unlink(missing_ok=True)
        command += ['--snapshot', str(args.snapshot.resolve())]
    if args.data:
        command += ['--data', str(args.data.resolve()), '--settings', str(args.settings.resolve()),
                    '--cache', str(args.cache.resolve())]
        if not legacy:
            command += ['--locale', args.locale]
        elif args.locale != 'enUS':
            parser.error('The legacy original-data fixture currently uses enUS')
    started = datetime.now(timezone.utc).isoformat()
    def execute(arguments):
        try:
            return subprocess.run(arguments, env=environment, capture_output=True,
                                  text=True, timeout=args.timeout), False
        except subprocess.TimeoutExpired as error:
            def partial(value):
                return value.decode('utf-8', errors='replace') if isinstance(value, bytes) else value or ''
            return subprocess.CompletedProcess(arguments, 124, partial(error.stdout),
                partial(error.stderr) + '\nOpenGL probe timed out; terminated, not a clean shutdown.\n'), True
    check_records = {}
    for key, check_binary, check_hash in checks:
        check_result, check_timeout = execute([str(check_binary.resolve())])
        print(check_result.stdout, end='')
        print(check_result.stderr, end='')
        check_records[key] = dict(binary_sha256=check_hash, exit_code=check_result.returncode,
                                 timed_out=check_timeout, stdout=check_result.stdout.splitlines(),
                                 stderr=check_result.stderr.splitlines())
    result, timed_out = execute(command)
    print(result.stdout, end='')
    print(result.stderr, end='')
    record = dict(schema_version=1, scope='M3 SDL3/Gx context and asset probe; no original login UI or gameplay',
                  started_at=started, completed_at=datetime.now(timezone.utc).isoformat(),
                  probe=probe_name, source_commit=manifest['source_commit'], workflow_run=manifest['workflow_run'],
                  binary_sha256=actual, preset=manifest['preset'], os=platform.platform(),
                  machine=platform.machine(), sdl3=manifest.get('sdl3'), video='x11',
                  original_data_requested=bool(args.data), locale=args.locale if args.data else None,
                  environment={key: environment.get(key) for key in ['SDL_VIDEODRIVER', 'EGL_PLATFORM',
                      'LIBGL_ALWAYS_SOFTWARE', 'ASAN_OPTIONS', 'UBSAN_OPTIONS', 'LSAN_OPTIONS', 'LD_PRELOAD', 'LD_AUDIT']},
                  exit_code=result.returncode, timed_out=timed_out,
                  stdout=result.stdout.splitlines(), stderr=result.stderr.splitlines())
    if check_records:
        record.update(check_records)
        record['probe_exit_code'] = result.returncode
        failures = [check['exit_code'] for check in check_records.values() if check['exit_code']]
        record['exit_code'] = failures[0] if failures else result.returncode
        record['timed_out'] = any(check['timed_out'] for check in check_records.values()) or timed_out
    args.output.write_text(json.dumps(record, indent=2) + '\n')
    raise SystemExit(record['exit_code'])


if __name__ == '__main__':
    main()
