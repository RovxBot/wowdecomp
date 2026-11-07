"""Source-only checks for the adaptations; no local client compilation."""
from pathlib import Path
import json
import sys


def audit(root):
    receipt = json.loads((root / 'source-audit.json').read_text())
    assert (root / 'CMakeLists.txt').read_text().startswith('# ')
    cmake = (root / 'CMakeLists.txt').read_text()
    assert cmake.index('add_compile_definitions(WOWDECOMP_ISOLATED_SERVER_BUILD=') < cmake.index('add_subdirectory(ThirdParty)')
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
    vk = (root / 'Engine/Source/Gx/CGxDeviceVulkan/VkContext.cpp').read_text()
    begin = vk.index('static bool MemoryTypeEnabled(')
    end = vk.index('int32_t VkContext::AcquireBuffer(', begin)
    image_begin = vk.index('int32_t VkContext::CreateImage(')
    image_end = vk.index('VkStaging VkContext::Stage(', image_begin)
    assert (root / 'RovNativeVulkanMemorySelect.inc').read_text() == vk[begin:end] + vk[image_begin:image_end]
    assert 'add_test(NAME rov_native_vulkan_memory COMMAND RovNativeVulkanMemoryCheck)' in cmake
    framebuffer = (root / 'RovNativeVulkanFramebuffer.inc').read_text()
    device = (root / 'Engine/Source/Gx/CGxDeviceVulkan/CGxDeviceVulkan.cpp').read_text()
    for source, signature in [(device, 'VkFramebuffer CGxDeviceVulkan::ITexFramebuffer('),
                              (vk, 'void VkContext::Barrier(')]:
        start = source.index(signature)
        opening = source.index('{', start)
        depth, end = 1, opening + 1
        while depth:
            depth += (source[end] == '{') - (source[end] == '}')
            end += 1
        assert source[start:end] in framebuffer
    assert 'add_test(NAME rov_native_vulkan_framebuffer COMMAND RovNativeVulkanFramebufferCheck)' in cmake
    scene = (root / 'Engine/Source/Model2/CM2SceneRender.cpp').read_text()
    batch = (root / 'RovNativeM2Batch.inc').read_text()
    for signature in ['void CM2SceneRender::SetBatchVertices(', 'void CM2SceneRender::DrawBatch()',
                      'static int32_t DrawCharacterRun(', 'void CM2SceneRender::Draw(M2PASS ']:
        start = scene.index(signature)
        opening = scene.index('{', start)
        depth, end = 1, opening + 1
        while depth:
            depth += (scene[end] == '{') - (scene[end] == '}')
            end += 1
        assert scene[start:end] in batch
    assert 'this->m_curType = -1;' in batch
    assert 'add_test(NAME rov_native_m2_batch COMMAND RovNativeM2BatchCheck)' in cmake
    for name in receipt['adaptations']:
        assert '// VERIFY:' not in (root / name).read_text(), name
    print('full-linux-source-audit=passed: pinned source, mandatory signatures/digest, explicit test gate, '
          'no autologin/agreement automation, no stale adapted VERIFY claims')


if __name__ == '__main__':
    audit(Path(sys.argv[1]))
