"""Native XML source/fixture policy checks, without client compilation."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
class NativeXMLPolicy(unittest.TestCase):
    def test_original_key_and_independent_fixture(self):
        def key(path, marker):
            body=(ROOT/path).read_text().split(marker,1)[1].split('};',1)[0]
            return bytes(int(n,16) for n in re.findall(r'0x[0-9a-fA-F]{2}\b',body))
        original=key('WoW/Source/Glue/CGlueMgr.cpp','const uint8_t InterfaceKey[256] = {')
        native=key('Engine/Source/FrameXML/Portable/InterfaceKey.h','interfaceKey[256] = {')
        self.assertEqual(len(native),256)
        self.assertEqual(native,original)
        fixture=json.loads((ROOT/'Tools/NativeXML/signature_fixture.json').read_text())
        modulus=bytes.fromhex(fixture['modulus'])
        self.assertNotEqual(original,modulus)
        spec=importlib.util.spec_from_file_location('xml_fixture',ROOT/'Tools/NativeXML/generate_fixtures.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        for name,content in [('Fixture.toc',b''.join(module.FILES[n] for n in module.ORDER)),
                             ('Missing.toc',module.FILES['Missing.toc'])]:
            data=bytes.fromhex(fixture[name+'.sig'])
            self.assertEqual(len(data),276)
            self.assertEqual(data[:16],hashlib.md5(content).digest())
            self.assertEqual(data[16:20],b'NGIS')
            decoded=pow(int.from_bytes(data[20:],'little'),65537,int.from_bytes(modulus,'little')).to_bytes(256,'little')
            self.assertEqual(decoded,hashlib.sha1(data[:16]+(name+'.sig').upper().encode()).digest()+b'\xbb'*235+b'\x0b')

    def test_driver_refuses_local_builds(self):
        env=os.environ.copy();env.pop('GITHUB_ACTIONS',None)
        result=subprocess.run([sys.executable,str(ROOT/'Tools/Porting/run_xml_checks.py'),'--preset','linux-xml'],env=env,capture_output=True,text=True)
        self.assertNotEqual(result.returncode,0)
        self.assertIn('compilation must run in GitHub Actions',result.stderr)

    def test_presets_retain_gate_and_signatures(self):
        presets={p['name']:p for p in json.loads((ROOT/'CMakePresets.json').read_text())['configurePresets']}
        for name in ['linux-xml','linux-xml-asan','macos-arm64-xml','windows-xml']:
            values={};current=presets[name]
            while current:
                values={**current.get('cacheVariables',{}),**values}
                parent=current.get('inherits');current=presets[parent] if parent else None
            self.assertEqual(values['WOWDECOMP_BUILD_CLIENT'],'OFF')
            self.assertEqual(values['WOWDECOMP_BUILD_NATIVE_XML'],'ON')
            self.assertEqual(values['WOWDECOMP_BUILD_NATIVE_FRAMESCRIPT'],'ON')
            self.assertNotEqual(values.get('WOWDECOMP_DISABLE_SIGNATURE_CHECKS'),'ON')
