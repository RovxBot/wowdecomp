#include "Engine/Source/Os/System.h"
#include "Engine/Source/Storm/H/storm.h"
#include <cstdint>
#include <cstdio>
#include <vector>
#if defined(WOWDECOMP_OS_LINUX)
#include <link.h>
#elif defined(WOWDECOMP_OS_MACOS)
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach/vm_prot.h>
#elif defined(WOWDECOMP_OS_WINDOWS)
#include <windows.h>
#endif

namespace {
struct Range { uintptr_t begin, size; };
// Like the original PE .text check, callbacks must belong to executable text in
// the main image. Shared-library/JIT callbacks are not admitted by this adapter.
const std::vector<Range>& TextRanges() {
    static const auto ranges = [] {
        std::vector<Range> result;
#if defined(WOWDECOMP_OS_LINUX)
        dl_iterate_phdr([](dl_phdr_info* info, size_t, void* state) {
            if (info->dlpi_name && *info->dlpi_name) return 0;
            auto& found = *static_cast<std::vector<Range>*>(state);
            for (size_t i = 0; i < info->dlpi_phnum; ++i) {
                const auto& header = info->dlpi_phdr[i];
                if (header.p_type == PT_LOAD && (header.p_flags & PF_X))
                    found.push_back({info->dlpi_addr + header.p_vaddr, header.p_memsz});
            }
            return 1;
        }, &result);
#elif defined(WOWDECOMP_OS_MACOS)
        const auto* header = reinterpret_cast<const mach_header_64*>(_dyld_get_image_header(0));
        if (header && header->magic == MH_MAGIC_64) {
            const auto slide = _dyld_get_image_vmaddr_slide(0);
            const auto* command = reinterpret_cast<const load_command*>(header + 1);
            for (uint32_t i = 0; i < header->ncmds; ++i) {
                if (command->cmd == LC_SEGMENT_64) {
                    const auto* segment = reinterpret_cast<const segment_command_64*>(command);
                    if (segment->initprot & VM_PROT_EXECUTE)
                        result.push_back({static_cast<uintptr_t>(segment->vmaddr + slide), static_cast<uintptr_t>(segment->vmsize)});
                }
                command = reinterpret_cast<const load_command*>(reinterpret_cast<const char*>(command) + command->cmdsize);
            }
        }
#elif defined(WOWDECOMP_OS_WINDOWS)
        const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
        const auto* section = IMAGE_FIRST_SECTION(nt);
        for (uint32_t i = 0; i < nt->FileHeader.NumberOfSections; ++i)
            if (section[i].Characteristics & IMAGE_SCN_MEM_EXECUTE)
                result.push_back({base + section[i].VirtualAddress, section[i].Misc.VirtualSize});
#endif
        return result;
    }();
    return ranges;
}
}

void OsValidateFunctionPointer(const void* pointer) {
    const auto address = reinterpret_cast<uintptr_t>(pointer);
    for (const auto& range : TextRanges())
        if (address >= range.begin && address - range.begin < range.size) return;
    char message[96];
    std::snprintf(message, sizeof(message), "Invalid function pointer: %p", pointer);
    SErrDisplayAppFatalMessage(message, __FILE__, __LINE__);
}
