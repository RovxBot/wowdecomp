"""Actions-only build/package entry for the opt-in integrated Linux startup."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--preset', choices=['linux-client', 'linux-client-asan'], required=True)
    args = parser.parse_args()
    if os.environ.get('GITHUB_ACTIONS') != 'true':
        raise SystemExit('All client/dependency compilation must run in GitHub Actions.')
    root = Path(__file__).resolve().parents[2]
    os.chdir(root)
    build = root / 'build' / args.preset
    output = root / 'artifacts' / args.preset
    output.mkdir(parents=True, exist_ok=True)
    manifest = {
        'schema_version': 1,
        'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
        'workflow_run': f"{os.environ['GITHUB_SERVER_URL']}/{os.environ['GITHUB_REPOSITORY']}/actions/runs/{os.environ['GITHUB_RUN_ID']}",
        'preset': args.preset, 'signatures': 'required',
        'target_os': 'LINUX', 'target_cpu': 'X86_64',
        'status': 'building', 'scope': 'integrated startup; original-data runtime evidence is separate',
    }
    try:
        for name, command in [
            ('configure', ['cmake', '--preset', args.preset]),
            ('compile', ['cmake', '--build', str(build), '--target', 'WowNativeClient', 'NativeStartupCheck', '--parallel', '4']),
            ('contracts', [str(build / 'bin/NativeStartupCheck')]),
        ]:
            with (output / f'{name}.log').open('w') as log:
                process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                for line in process.stdout:
                    print(line, end='', flush=True)
                    log.write(line)
                if process.wait():
                    raise RuntimeError(f'{name} failed')
        manifest['binaries'] = {}
        for name in ['WowNativeClient', 'NativeStartupCheck']:
            shutil.copy2(build / 'bin' / name, output / name)
            manifest['binaries'][name] = hashlib.sha256((output / name).read_bytes()).hexdigest()
        notices = output / 'notices'
        notices.mkdir(exist_ok=True)
        for source, name in [
            ('ThirdParty/bzip2/LICENSE', 'bzip2-LICENSE.txt'),
            ('Engine/Source/lua-5.1/COPYRIGHT', 'Lua-LICENSE.txt'),
            ('ThirdParty/expat/COPYING', 'Expat-LICENSE.txt'),
            ('ThirdParty/freetype/docs/FTL.txt', 'FreeType-LICENSE.txt'),
            ('ThirdParty/freetype/docs/NATIVE_ADAPTATIONS.md', 'FreeType-NATIVE_ADAPTATIONS.md'),
        ]:
            shutil.copy2(root / source, notices / name)
        for source, name in [('ThirdParty/zlib/zlib.h', 'zlib-NOTICE.txt'),
                             ('ThirdParty/zlib/contrib/blast/blast.h', 'blast-NOTICE.txt'),
                             ('ThirdParty/lzma/LzmaDecode.c', 'LZMA-NOTICE.txt')]:
            content = (root / source).read_text()
            (notices / name).write_text(content[:content.index('*/') + 2] + '\n')
        shutil.copy2(build / '_deps/sdl3-src/LICENSE.txt', notices / 'SDL3-LICENSE.txt')
        manifest['notices'] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(notices.iterdir())}
        manifest['status'] = 'build-and-contracts-passed'
    except Exception as error:
        manifest['status'] = 'failed'
        manifest['failure'] = str(error)
        raise
    finally:
        (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')


if __name__ == '__main__':
    main()
