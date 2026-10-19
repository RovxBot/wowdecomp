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
                         ('PortableCoreCheck', 'EXECUTABLE', 'Tools/PortableCore/hashes.cpp'),
                         ('WowCoreServices', 'STATIC_LIBRARY', 'Engine/Source/Os/Portable/Sync.cpp'),
                         ('CoreServicesCheck', 'EXECUTABLE', 'Tools/PortableCore/runtime.cpp'),
                         ('NativeCore', 'EXECUTABLE', 'Tools/NativeCore/main.cpp')]]

    def test_clean_core(self):
        self.assertEqual(audit(self.targets, 'core', 'LINUX', 'X86_64'), [])

    def test_packet_subset_and_reject_wrong_provider(self):
        targets=copy.deepcopy(self.targets)
        for name,sources in [('WowNativePackets',['Engine/Source/Base/CDataStore.cpp',
                'Engine/Source/Storm/Source/SArc4.cpp','Engine/Source/BlizzardCore/Source/Packages/Util/SHA1.cpp']),
                ('NativePacketCheck',['Tools/NativeNet/packets.cpp'])]:
            target=copy.deepcopy(targets[0]);target['name']=name
            target['sources']=[{'path':source} for source in sources]
            target['compileGroups'][0]['defines'].append({'define':'WOWDECOMP_PORTABLE_RUNTIME=1'})
            targets.append(target)
        self.assertEqual(audit(targets,'packets','LINUX','X86_64'),[])
        targets[-2]['sources'].append({'path':'WoW/Source/WowServices/Win64/WowConnection.cpp'})
        self.assertTrue(audit(targets,'packets','LINUX','X86_64'))

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

    def file_targets(self):
        targets = copy.deepcopy(self.targets)
        for name, sources in [
            ('WowNativeFiles', ['Engine/Source/BlizzardCore/Portable/File.cpp',
                                'Engine/Source/BlizzardCore/Portable/FilePaths.cpp',
                                'Engine/Source/BlizzardCore/Portable/FilePosix.cpp',
                                'Engine/Source/Os/Portable/File.cpp']),
            ('FileServicesCheck', ['Tools/PortableFiles/files.cpp', 'Tools/PortableFiles/interrupted.cpp'])]:
            target = copy.deepcopy(targets[0])
            target['name'] = name
            target['sources'] = [{'path': path} for path in sources]
            target['compileGroups'][0]['defines'].append({'define': 'WOWDECOMP_PORTABLE_RUNTIME=1'})
            targets.append(target)
        return targets

    def test_native_files_allowlisted(self):
        self.assertEqual(audit(self.file_targets(), 'files', 'LINUX', 'X86_64'), [])
        self.assertTrue(audit(self.file_targets(), 'core', 'LINUX', 'X86_64'))

    def test_native_files_reject_wrong_or_extra_provider(self):
        for path in ['Engine/Source/BlizzardCore/Portable/FileWindows.cpp',
                     'Engine/Source/BlizzardCore/Source/System/File/Win64/Filestack.cpp']:
            targets = self.file_targets()
            targets[-2]['sources'].append({'path': path})
            self.assertTrue(audit(targets, 'files', 'LINUX', 'X86_64'))

    def test_native_files_require_native_abi(self):
        targets = self.file_targets()
        targets[-2]['compileGroups'][0]['defines'].pop()
        self.assertTrue(audit(targets, 'files', 'LINUX', 'X86_64'))

    def test_sdl_control_allows_build_utilities_but_rejects_engine_libraries(self):
        targets = copy.deepcopy(self.targets)
        targets[2]['id'] = 'WowCoreServices::@fixture'
        control = copy.deepcopy(targets[0])
        control.update(name='SDLDesktopControl', type='EXECUTABLE',
                       sources=[{'path': 'Tools/PortableWindow/desktop_control.cpp'}],
                       dependencies=[{'id': 'ZERO_CHECK::@fixture'}, {'id': 'SDL3-static::@fixture'}])
        targets += [dict(name='ZERO_CHECK', id='ZERO_CHECK::@fixture', type='UTILITY'), control]
        message = 'SDLDesktopControl: engine dependency leaked into the SDL-only control'
        # Other window targets are intentionally absent from this narrow fixture.
        self.assertNotIn(message, audit(targets, 'window', 'LINUX', 'X86_64'))
        control['dependencies'].append({'id': 'WowCoreServices::@fixture'})
        self.assertIn(message, audit(targets, 'window', 'LINUX', 'X86_64'))


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
