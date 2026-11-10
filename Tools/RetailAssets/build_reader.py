#!/usr/bin/env python3
"""Build the optional offline asset reader in Actions; never takes game data."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import urllib.request


def sha256(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    if os.environ.get('GITHUB_ACTIONS') != 'true':
        raise SystemExit('Reader compilation must run in GitHub Actions')
    lock = json.loads(Path(__file__).with_name('reader.json').read_text())
    work = Path(os.environ['RUNNER_TEMP']) / 'forever-casc-reader'
    work.mkdir()
    archive = work / 'source.tar.gz'
    url = f"https://codeload.github.com/{lock['repository']}/tar.gz/{lock['commit']}"
    with urllib.request.urlopen(url, timeout=120) as response, archive.open('wb') as out:
        shutil.copyfileobj(response, out)
    if sha256(archive) != lock['archive_sha256']:
        raise SystemExit('Reader source checksum mismatch')
    with tarfile.open(archive) as source:
        source.extractall(work, filter='data')
    root = work / f"CascLib-{lock['commit']}"
    # Upstream probes .build.info with O_RDWR despite only checking existence.
    # Narrow read-only adaptation; the library must work on a read-only mount.
    path = root / 'src/CascFiles.cpp'
    original = path.read_bytes()
    adapted = original
    for argument in (b'szLocalPath', b'szFileName'):
        old = b'FileStream_OpenFile(' + argument + b', 0)'
        if adapted.count(old) != 1:
            raise SystemExit('Read-only probe adaptation no longer matches pinned source')
        adapted = adapted.replace(old, b'FileStream_OpenFile(' + argument + b', STREAM_FLAG_READ_ONLY)')
    path.write_bytes(adapted)
    build = work / 'build'
    subprocess.run(['cmake', '-S', str(root), '-B', str(build), '-G', 'Ninja',
                    '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_POLICY_VERSION_MINIMUM=3.5',
                    '-DCASC_BUILD_SHARED_LIB=ON', '-DCASC_BUILD_STATIC_LIB=OFF',
                    '-DCASC_BUILD_TESTS=OFF'], check=True)
    subprocess.run(['cmake', '--build', str(build), '--target', 'casc', '-j', '2'], check=True)
    out = Path('artifacts/retail-asset-reader')
    out.mkdir(parents=True)
    shutil.copyfile(build / lock['library'], out / lock['library'])
    shutil.copyfile(root / 'LICENSE', out / 'LICENSE.CascLib')
    manifest = {'schema': 1, 'source': lock,
                'adaptation': 'CascFiles.cpp CheckCascBuildFileExact/FileAlreadyExists: read-only existence probes',
                'adapted_CascFiles_sha256': sha256(path),
                'product_commit': os.environ['GITHUB_SHA'],
                'run_id': os.environ['GITHUB_RUN_ID'],
                'run_attempt': os.environ['GITHUB_RUN_ATTEMPT'],
                'files': {p.name: sha256(p) for p in out.iterdir()}}
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')


if __name__ == '__main__':
    main()
