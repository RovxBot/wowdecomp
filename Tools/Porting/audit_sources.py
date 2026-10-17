#!/usr/bin/env python3
"""Inventory tracked portability dependencies without compiling or executing them.

Comment/whitespace-normalized comparisons are a triage aid, not proof of C++
semantic equivalence. Use pinned, separate checkouts for reproducible comparisons.
"""

import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path

DONOR_COMMIT = "959b0b85c5f55c565328d7c3ca33027a65e41327"
SOURCE_SUFFIXES = {".c", ".cpp", ".h", ".hpp", ".inl"}
ROOTS = ("Engine", "WoW", "Tools", "ThirdParty")
BINARY_DEPENDENCIES = (
    "ThirdParty/nvapi/amd64/nvapi64.lib",
    "ThirdParty/nvapi/x86/nvapi.lib",
    "ThirdParty/xvidcore/lib/x64/libxvidcore.lib",
)
MODULES = (
    "Engine/Source/Os/",
    "Engine/Source/Storm/",
    "Engine/Source/BlizzardCore/",
    "Engine/Source/Gx/",
    "Engine/Source/ShaderEffect/",
    "Engine/Source/SoundEngine/",
    "Engine/Source/Model2/",
    "WoW/Source/Net/",
    "WoW/Source/WorldClient/",
    "WoW/Source/LiquidSystem/",
    "WoW/Source/Ui/",
)

# Preserve string/character/raw literals containing comment markers.
LITERAL = (
    r'(?:u8|u|U|L)?R"(?P<delimiter>[^ ()\\\t\r\n]{0,16})\([\s\S]*?\)(?P=delimiter)"'
    r'|(?:u8|u|U|L)?"(?:\\[\s\S]|[^"\\])*"'
    r"|(?:u8|u|U|L)?'(?:\\[\s\S]|[^'\\])*'"
)
COMMENTS_AND_LITERALS = re.compile(LITERAL + r"|/\*[\s\S]*?\*/|//[^\r\n]*")
TOKENS = re.compile(
    LITERAL + r"|[A-Za-z_]\w*|(?:\d|\.\d)(?:[\w.]|(?<=[eEpP])[+-])*"
    r"|<<=|>>=|<=>|->\*|\.\.\.|::|->|\.\*|\+\+|--|&&|\|\||<<|>>|"
    r"<=|>=|==|!=|\+=|-=|\*=|/=|%=|&=|\|=|\^=|##|[^\s]"
)
INDICATORS = {
    "windows_headers": re.compile(
        r"#\s*include\s*[<\"](?:windows|winsock2?|mswsock|ws2tcpip|d3d9|d3d11|"
        r"dxgi|dinput|dsound|wininet|shellapi|mmsystem|setupapi)\.h", re.I
    ),
    "windows_api_or_types": re.compile(
        r"\b(?:HWND|HANDLE|HDC|HGLRC|CRITICAL_SECTION|DWORD|OVERLAPPED|"
        r"CreateFile[AW]?|CreateThread|CreateEvent[AW]?|WaitForMultipleObjects|"
        r"CreateIoCompletionPort|GetQueuedCompletionStatus|Interlocked\w*|"
        r"VirtualAlloc|GetProcAddress|LoadLibrary[AW]?|wgl\w+)\b"
    ),
    "msvc_extensions": re.compile(
        r"\b(?:__try|__except|__declspec|__fastcall|__thiscall|__stdcall|"
        r"_stricmp|_snprintf_s|_aligned_malloc)\b"
    ),
    "x86_instructions_or_intrinsics": re.compile(
        r"\b(?:__cpuid\w*|__readeflags|__writeeflags|__rdtsc|_mm_\w+|__asm)\b|"
        r"#\s*include\s*[<\"](?:xmmintrin|emmintrin|immintrin|mmintrin)\.h"
    ),
}


def git(root, *args):
    return subprocess.check_output(["git", "-C", str(root), *args])


def tracked_files(root):
    return sorted(
        p.decode("utf-8") for p in git(root, "ls-files", "-z", "--", *ROOTS).split(b"\0") if p
    )


def strip_comments(text):
    return COMMENTS_AND_LITERALS.sub(
        lambda m: " " if m.group().startswith(("//", "/*")) else m.group(), text
    )


def normalized(text):
    return tuple(m.group() for m in TOKENS.finditer(strip_comments(text)))


def digest(data):
    return hashlib.sha256(data).hexdigest()


def inventory(root):
    paths = tracked_files(root)
    sources = {p: (root / p).read_text(errors="replace")
               for p in paths if Path(p).suffix in SOURCE_SUFFIXES}
    clean = {p: strip_comments(text) for p, text in sources.items()}
    indicators = {name: sorted(p for p, text in clean.items() if pattern.search(text))
                  for name, pattern in INDICATORS.items()}
    binaries = []
    for name in BINARY_DEPENDENCIES:
        path = root / name
        binaries.append({"path": name, "tracked": name in paths, "present": path.is_file(),
                         **({"bytes": path.stat().st_size, "sha256": digest(path.read_bytes())}
                            if path.is_file() else {})})
    cmake = {}
    for name in ["CMakeLists.txt"] + [p for p in paths if p.endswith("/CMakeLists.txt")]:
        text = (root / name).read_text()
        links = re.findall(r"target_link_libraries\s*\(([^)]+)\)", text, re.I)
        cmake[name] = [block.split() for block in links]
    return {
        "commit": git(root, "rev-parse", "HEAD").decode().strip(),
        "tracked_source_files": len(sources),
        "indicators": {key: {"count": len(value), "paths": value} for key, value in indicators.items()},
        "binary_dependencies": binaries,
        "cmake_link_blocks": cmake,
        "platform_directories": sorted({str(Path(p).parent) for p in sources
                                        if re.search(r"/(Win32|Win64|win32|Linux|linux|Mac|macOS|Posix)/", p)}),
    }, sources


def compare(left, right):
    result = {}
    for module in MODULES:
        old = {p: v for p, v in left.items() if p.startswith(module)}
        new = {p: v for p, v in right.items() if p.startswith(module)}
        groups = {"identical": [], "comments_or_whitespace_only": [], "changed_tokens": [],
                  "donor_only": sorted(new.keys() - old.keys()),
                  "baseline_only": sorted(old.keys() - new.keys())}
        for name in sorted(old.keys() & new.keys()):
            if old[name] == new[name]:
                group = "identical"
            elif normalized(old[name]) == normalized(new[name]):
                group = "comments_or_whitespace_only"
            else:
                group = "changed_tokens"
            groups[group].append(name)
        result[module] = {"counts": {k: len(v) for k, v in groups.items()}, "paths": groups}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    baseline, left = inventory(args.source)
    donor, right = inventory(args.reference)
    if donor["commit"] != DONOR_COMMIT:
        parser.error(f"Expected donor {DONOR_COMMIT}, got {donor['commit']}")
    changes = git(args.source, "diff", "HEAD", "--name-only", "--", *ROOTS).decode().splitlines()
    report = {
        "schema_version": 1,
        "method": "Tracked C/C++/header/inl files; regex indicators after comment removal; token comparison is heuristic, not semantic verification.",
        "baseline_tracked_changes": changes,
        "baseline": baseline,
        "donor": donor,
        "comparison": compare(left, right),
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    print(f"Audited {baseline['tracked_source_files']} baseline and {donor['tracked_source_files']} donor source files.")
    print(f"Source: {baseline['commit']}; donor: {donor['commit']}")
    print(f"Wrote {args.output}")


if __name__ == "__main__":
    main()
