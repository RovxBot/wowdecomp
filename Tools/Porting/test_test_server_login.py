"""Release profile/package policies; no client or dependency compilation."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import test_native_d3d9_client as packages
from run_native_client import require_test_server_artifact, verify_artifact

ROOT = Path(__file__).resolve().parents[2]

class TestServerLoginPolicyTests(unittest.TestCase):
    def test_all_normal_presets_exclude_test_allowance(self):
        for preset in json.loads((ROOT/'CMakePresets.json').read_text())['configurePresets']:
            self.assertNotIn('WOWDECOMP_NATIVE_TEST_SERVER_LOGIN', preset.get('cacheVariables',{}))

    def test_launcher_refuses_profile_with_normal_artifact(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            packages.D3d9ClientPackageTests().package(root)
            manifest,_=verify_artifact(root,False,'dxvk')
            with self.assertRaisesRegex(ValueError,'requires a signature-enabled TEST-SERVER DXVK artifact'):
                require_test_server_artifact(manifest,'dxvk')
            manifest['test_server_login']=True
            require_test_server_artifact(manifest,'dxvk')
            with self.assertRaises(ValueError): require_test_server_artifact(manifest,'opengl')

    def test_test_build_drivers_refuse_local_compilation(self):
        env=os.environ.copy();env.pop('GITHUB_ACTIONS',None)
        for script,args in [('Tools/NativeD3D9/build.py',['--client','--test-server-login']),
                ('Tools/Porting/run_connection_checks.py',['--preset','linux-connections','--test-server-login'])]:
            result=subprocess.run([sys.executable,str(ROOT/script),*args],env=env,capture_output=True,text=True)
            self.assertNotEqual(result.returncode,0)
            self.assertIn('compilation must run in GitHub Actions',result.stderr)

if __name__=='__main__': unittest.main()
