"""Asset-free audit fixtures; these tests never compile client code."""
import copy
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from audit_build_graph import audit


class BuildGraphTests(unittest.TestCase):
    def setUp(self):
        self.targets = [dict(name=name, type=kind, sources=[{'path': source}],
                             compileGroups=[{'defines': [{'define': d} for d in
                                            ['WOWDECOMP_OS_LINUX=1', 'WOWDECOMP_CPU_X86_64=1',
                                             'WOWDECOMP_ARCH_64=1']]}]) for name, kind, source in
                        [('WowPortableCore', 'STATIC_LIBRARY', 'Engine/Source/Storm/Source/MD5.cpp'),
                         ('PortableCoreCheck', 'EXECUTABLE', 'Tools/PortableCore/hashes.cpp')]]

    def test_clean_core(self):
        self.assertEqual(audit(self.targets, 'core', 'LINUX', 'X86_64'), [])

    def test_reject_windows_leaks_and_wrong_identity(self):
        for source in ['Engine/Source/Os/Win64/Time.cpp', 'Engine/Source/Os/Win32/Time.cpp',
                       'waystone.rc', 'dependency.lib']:
            with self.subTest(source=source):
                targets = copy.deepcopy(self.targets)
                targets[0]['sources'].append({'path': source})
                self.assertTrue(audit(targets, 'core', 'LINUX', 'X86_64'))
        for definition in ['WIN32', '_WINDOWS=1', '_USE_32BIT_TIME_T']:
            targets = copy.deepcopy(self.targets)
            targets[0]['compileGroups'][0]['defines'].append({'define': definition})
            self.assertTrue(audit(targets, 'core', 'LINUX', 'X86_64'))
        self.assertTrue(audit(self.targets, 'core', 'MACOS', 'ARM64'))

    def test_reject_extra_client_target(self):
        self.targets.append(dict(name='Wow', type='EXECUTABLE'))
        self.assertTrue(audit(self.targets, 'core', 'LINUX', 'X86_64'))

    def test_windows_core_can_link_its_own_static_library(self):
        for target in self.targets:
            target['compileGroups'][0]['defines'][0]['define'] = 'WOWDECOMP_OS_WINDOWS=1'
        self.targets[1]['link'] = {'commandFragments': [{'fragment': 'WowPortableCore.lib'}]}
        self.assertEqual(audit(self.targets, 'core', 'WINDOWS', 'X86_64'), [])
        self.targets[1]['link']['commandFragments'].append({'fragment': '/ENTRY:start'})
        self.assertTrue(audit(self.targets, 'core', 'WINDOWS', 'X86_64'))


@unittest.skipUnless(shutil.which('cmake'), 'CMake unavailable; source-selection checks run in Actions')
class SourceSelectionTests(unittest.TestCase):
    def test_windows_trees_are_selected_explicitly(self):
        # LANGUAGES NONE intentionally avoids compiler detection/compilation.
        selector = Path(__file__).resolve().parents[2] / 'cmake/SelectSources.cmake'
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / 'source'
            source.mkdir()
            # CMake expands Windows 8.3 names such as RUNNER~1.
            source = source.resolve()
            files = ['common.cpp', 'nested/common.cpp', 'Win32/os.cpp', 'Win64/os.cpp',
                     'nested/Win32/service.cpp', 'nested/Win64/service.cpp',
                     'nested/Windows/feature.cpp', 'nested/Linux/service.cpp',
                     'nested/Mac/service.cpp', 'nested/Posix/service.cpp']
            for file in files:
                path = source / file
                path.parent.mkdir(parents=True, exist_ok=True)
                path.touch()
            (source / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.21)\nproject(SourceSelection LANGUAGES NONE)\n'
                f'include("{selector.as_posix()}")\n'
                'wowdecomp_collect_sources(selected)\n'
                'file(WRITE "${CMAKE_BINARY_DIR}/selected.txt" "${selected}")\n')
            for tree in ['Win32', 'Win64']:
                build = Path(tmp) / tree
                result = subprocess.run(['cmake', '-S', str(source), '-B', str(build),
                                         f'-DWOWDECOMP_PLATFORM_SOURCE_DIRS={tree};Windows'],
                                        capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                selected = {Path(p).resolve().relative_to(source).as_posix()
                            for p in (build / 'selected.txt').read_text().split(';')}
                self.assertEqual(selected, {'common.cpp', 'nested/common.cpp', f'{tree}/os.cpp',
                                            f'nested/{tree}/service.cpp', 'nested/Windows/feature.cpp'})


if __name__ == '__main__':
    unittest.main()
