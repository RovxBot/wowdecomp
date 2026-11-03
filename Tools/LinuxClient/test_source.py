"""Source-only checks for the adaptations; no local client compilation."""
from pathlib import Path
import json
import sys


def audit(root):
    receipt = json.loads((root / 'source-audit.json').read_text())
    assert (root / 'CMakeLists.txt').read_text().startswith('# ')
    assert receipt['upstream']['commit'] == 'cb54f58119b88c8a3cfda6ad8d64e747dc65df7d'
    glue = (root / 'WoW/Source/Glue/CGlueMgr.cpp').read_text()
    assert 'AutoLoginPrepareGlue' not in glue and 'MaybeAutoLogin' not in glue
    assert 'wowdecomp-autologin.txt' not in glue and 'WOWDECOMP_AUTOLOGIN' not in glue
    assert 'DeleteInterfaceFiles();' not in glue
    assert 'InstallExtendedViewUiPatch();' not in glue
    assert 'memcmp(expectedDigest, digest, sizeof(digest))' in glue
    assert 'RovNative::GlueBegin();' in glue and 'RovNative::GlueEnd();' in glue
    assert 'status.m_highestSeverity >= STATUS_ERROR' in glue
    assert 'if (!RovNative::TestServerEnabled())' in glue
    scan = (root / 'WoW/Source/Glue/ScanDLLGlue.cpp').read_text()
    start = scan.index('void CGlueMgr::ScanDLLContinueAnyway()')
    end = scan.index('static uint32_t STORMAPI ScanThreadProc', start)
    assert 'm_scandllOkayToLogIn = 1' not in scan[start:end]
    assert 'return false;' in scan[start:end]
    for name in ['WoW/Source/Game/GameClient/Linux/WardenClient.cpp', 'WoW/Source/Linux/BotCheck.cpp']:
        assert 'Connection()->Disconnect();' in (root / name).read_text()
    assert '#define DISABLE_SIGNATURE_CHECKS' not in (root / 'RovNativePolicy.h').read_text()
    for name in receipt['adaptations']:
        assert '// VERIFY:' not in (root / name).read_text(), name
    print('full-linux-source-audit=passed: pinned source, mandatory signatures/digest, explicit test gate, '
          'no autologin/agreement automation, no stale adapted VERIFY claims')


if __name__ == '__main__':
    audit(Path(sys.argv[1]))
