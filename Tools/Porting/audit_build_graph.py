"""Check actual CMake File API source/definition selection; no compilation here."""
import argparse
import json
from pathlib import Path


def audit(targets, mode, target_os, cpu):
    errors = []
    compiled = [t for t in targets if t['type'] in
                ('STATIC_LIBRARY', 'SHARED_LIBRARY', 'EXECUTABLE', 'OBJECT_LIBRARY')]
    names = {t['name'] for t in compiled}
    if mode == 'core' and names != {'WowPortableCore', 'PortableCoreCheck'}:
        errors.append(f'Unexpected core targets: {sorted(names)}')
    if mode == 'windows' and not {'Wow', 'WowClient', 'Os', 'Storm', 'BlizzardCore'} <= names:
        errors.append('Windows reference targets are missing')
    expected_platform = 'Win64' if cpu == 'X86_64' else 'Win32'
    for target in compiled:
        sources = [s['path'].replace('\\', '/') for s in target.get('sources', [])]
        definitions = {d['define'] for g in target.get('compileGroups', [])
                       for d in g.get('defines', [])}
        required = {f'WOWDECOMP_OS_{target_os}=1', f'WOWDECOMP_CPU_{cpu}=1',
                    'WOWDECOMP_ARCH_64=1' if cpu != 'X86' else 'WOWDECOMP_ARCH_32=1'}
        if not required <= definitions:
            errors.append(f"{target['name']}: missing target identity: {sorted(required - definitions)}")
        for source in sources:
            parts = Path(source).parts
            if mode == 'core' and (set(parts) & {'Win32', 'Win64', 'Windows', 'win32'}
                                   or source.endswith(('.rc', '.def', '.lib'))):
                errors.append(f"{target['name']}: Windows source in core: {source}")
            if mode == 'windows' and (('Win32' in parts and expected_platform != 'Win32')
                                      or ('Win64' in parts and expected_platform != 'Win64')):
                errors.append(f"{target['name']}: wrong Windows source: {source}")
        if mode == 'core':
            forbidden = {'_USE_32BIT_TIME_T', 'DISABLE_SIGNATURE_CHECKS'}
            if target_os != 'WINDOWS':
                forbidden |= {'WIN32', '_WINDOWS', 'NOMINMAX', 'WIN32_LEAN_AND_MEAN',
                              '_CRT_SECURE_NO_WARNINGS'}
            if {d.split('=')[0] for d in definitions} & forbidden:
                errors.append(f"{target['name']}: Windows/client definitions leaked into core")
            for fragment in target.get('link', {}).get('commandFragments', []):
                if ((target_os != 'WINDOWS' and '.lib' in fragment['fragment'])
                        or '/ENTRY:' in fragment['fragment']):
                    errors.append(f"{target['name']}: Windows link flags in core")
        if mode == 'windows' and target['name'] in {'Wow', 'WowClient'}:
            if not any(s.endswith(f'/{expected_platform}/WinMain.cpp') for s in sources):
                errors.append(f"{target['name']}: selected Windows entry missing")
            if not any(s.endswith('waystone.rc') for s in sources):
                errors.append(f"{target['name']}: Windows resource missing")
    return errors


def read_targets(build_dir):
    reply = build_dir / '.cmake/api/v1/reply'
    indexes = sorted(reply.glob('index-*.json'))
    index = json.loads(indexes[-1].read_text())
    model_file = index['reply']['codemodel-v2']['jsonFile']
    model = json.loads((reply / model_file).read_text())
    configuration = next((c for c in model['configurations'] if c['name'] == 'Release'),
                         model['configurations'][0])
    return [json.loads((reply / t['jsonFile']).read_text()) for t in configuration['targets']]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--mode', choices=['core', 'windows'], required=True)
    parser.add_argument('--os', required=True)
    parser.add_argument('--cpu', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    targets = read_targets(args.build_dir)
    errors = audit(targets, args.mode, args.os, args.cpu)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps({'mode': args.mode, 'os': args.os, 'cpu': args.cpu,
                                     'errors': errors, 'targets': targets}, indent=2) + '\n')
    if errors:
        raise SystemExit('\n'.join(errors))
    print(f'Build graph passed: {args.mode}, {args.os}/{args.cpu}')


if __name__ == '__main__':
    main()
