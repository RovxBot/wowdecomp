"""Source-only graph tests; no Blizzard exports or client compilation."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location('retail_audit', Path(__file__).parent / 'RetailUI/audit_reference.py')
AUDIT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(AUDIT)


class ReferenceGraphTests(unittest.TestCase):
    def test_camelot_family_and_game_conditions(self):
        self.assertTrue(AUDIT.selected('[Family]\\Templates.xml [AllowLoadGameType mainline]'))
        self.assertTrue(AUDIT.selected('[Game]\\Templates.xml [AllowLoadGameType standard, camelot]'))
        self.assertFalse(AUDIT.selected('Wrath\\Templates.xml [AllowLoadGameType wrath, cata]'))
        self.assertFalse(AUDIT.selected('No.xml [ExcludeLoadGameType camelot]'))
        self.assertFalse(AUDIT.selected('Glue.xml [AllowLoad Glue]'))

    def test_transitive_edges_identity_and_implicit_providers(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            addons = root / 'Interface/AddOns'
            edit = addons / 'Blizzard_EditMode'
            dep = addons / 'Dependency'
            (edit / 'Camelot').mkdir(parents=True)
            (edit / 'Mainline').mkdir()
            dep.mkdir()
            (edit / 'Blizzard_EditMode.toc').write_text('## Dependencies: Dependency\n[Game]\\Input.lua [AllowLoadGameType camelot]\n[Family]\\Templates.xml\nNo.lua [AllowLoadGameType wrath]\n')
            (dep / 'Dependency.toc').write_text('## Dep: Blizzard_EditMode\n## OptionalDep: Optional\nProvider.xml\n')
            (dep / 'Provider.xml').write_text('<Ui><Frame name="Base" virtual="true"/></Ui>')
            (edit / 'Camelot/Input.lua').write_text('C_Sample.Read(); x:Run()')
            (edit / 'Mainline/Templates.xml').write_text('<Ui><Include file="../Child.xml"/><Frame inherits="Base" mixin="Example"/></Ui>')
            (edit / 'Child.xml').write_text('<Ui/>')
            report = AUDIT.audit(root)
            self.assertEqual(len(report['files']), 6)
            self.assertFalse(report['missing_files'])
            self.assertEqual(len(report['cycles']), 1)
            self.assertEqual(len(report['optional_addons']), 1)
            self.assertIn('C_Sample.Read', report['namespaced_calls'])
            self.assertEqual(report['templates']['Base']['providers'], ['Interface/AddOns/Dependency/Provider.xml'])
            self.assertTrue(any(e['kind'] == 'xml-include' for e in report['edges']))
            old = report['fingerprint']
            (edit / 'Child.xml').write_text('<Ui> </Ui>')
            self.assertNotEqual(AUDIT.audit(root)['fingerprint'], old)

    def test_missing_is_reported_and_escape_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            addon = root / 'Interface/AddOns/Blizzard_EditMode'
            addon.mkdir(parents=True)
            toc = addon / 'Blizzard_EditMode.toc'
            toc.write_text('Missing.lua\n')
            self.assertEqual(AUDIT.audit(root)['missing_files'], ['Interface/AddOns/Blizzard_EditMode/Missing.lua'])
            toc.write_text('../../../../outside.lua\n')
            with self.assertRaisesRegex(ValueError, 'escapes'):
                AUDIT.audit(root)


if __name__ == '__main__':
    unittest.main()
