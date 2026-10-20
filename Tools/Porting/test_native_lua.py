"""Source/policy checks only; no native compilation in these tests."""
import json
import os
from pathlib import Path
import subprocess
import sys
import unittest

ROOT=Path(__file__).resolve().parents[2]
class NativeLuaPolicyTests(unittest.TestCase):
    def test_driver_refuses_local_compilation(self):
        env=os.environ.copy();env.pop('GITHUB_ACTIONS',None)
        result=subprocess.run([sys.executable,str(ROOT/'Tools/Porting/run_lua_checks.py'),
            '--preset','linux-lua'],env=env,capture_output=True,text=True)
        self.assertNotEqual(result.returncode,0)
        self.assertIn('compilation must run in GitHub Actions',result.stderr)

    def test_lua_presets_keep_client_gate(self):
        presets={r['name']:r for r in json.loads((ROOT/'CMakePresets.json').read_text())['configurePresets']}
        for name in ['linux-lua','linux-lua-asan','macos-arm64-lua','windows-lua']:
            merged={};current=presets[name]
            while current:
                merged={**current.get('cacheVariables',{}),**merged}
                parent=current.get('inherits');current=presets[parent] if parent else None
            self.assertEqual(merged['WOWDECOMP_BUILD_CLIENT'],'OFF')
            self.assertEqual(merged['WOWDECOMP_BUILD_NATIVE_LUA'],'ON')
        self.assertEqual(presets['macos-arm64-core']['cacheVariables']['CMAKE_OSX_ARCHITECTURES'],'arm64')
