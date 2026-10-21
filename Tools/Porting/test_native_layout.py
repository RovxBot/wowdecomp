"""Native layout build-policy checks; no local client/dependency compilation."""
import json
import os
from pathlib import Path
import subprocess
import sys
import unittest
ROOT=Path(__file__).resolve().parents[2]
class LayoutPolicy(unittest.TestCase):
    def test_driver_refuses_local_builds(self):
        env=os.environ.copy();env.pop('GITHUB_ACTIONS',None)
        result=subprocess.run([sys.executable,str(ROOT/'Tools/Porting/run_layout_checks.py'),'--preset','linux-layout'],env=env,capture_output=True,text=True)
        self.assertNotEqual(result.returncode,0)
        self.assertIn('compilation must run in GitHub Actions',result.stderr)

    def test_presets_retain_client_gate_and_signatures(self):
        presets={p['name']:p for p in json.loads((ROOT/'CMakePresets.json').read_text())['configurePresets']}
        for name in ['linux-layout','linux-layout-asan','macos-arm64-layout','windows-layout']:
            values={};current=presets[name]
            while current:
                values={**current.get('cacheVariables',{}),**values}
                parent=current.get('inherits');current=presets[parent] if parent else None
            self.assertEqual(values['WOWDECOMP_BUILD_CLIENT'],'OFF')
            self.assertEqual(values['WOWDECOMP_BUILD_NATIVE_LAYOUT'],'ON')
            self.assertEqual(values['WOWDECOMP_BUILD_NATIVE_XML'],'ON')
            self.assertNotEqual(values.get('WOWDECOMP_DISABLE_SIGNATURE_CHECKS'),'ON')
