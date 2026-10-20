# Original XML trees and interface signatures

This follows the qualified [FrameScript owner](FRAMESCRIPT.md). The separate
native target selects original `FrameXML/XMLTree.cpp`, `CDataAllocator`,
`RCString` and `TRefCnt`, with the repository's bundled Expat 1.95.5. It adapts
`LoadXML.cpp`'s original manifest/signature traversal at the native file boundary.
No donor import, host XML library or game data is added. Original OFFSET/VERIFY
comments describe historical Windows bodies; native branches are adaptations.

The XML runtime shares FrameScript's owner thread and completed Interface reader.
It pins that VM against reset/destruction until XML shutdown. Close trees and all
borrowed nodes/strings before the XML runtime; then close FrameScript, retained
file queue and packages. Parser/node/string pools are cleared after the final
tree. Foreign API mutation and cleanup with live trees are rejected. Original
attribute lookup, body/child offsets, node recycling and user-data Release remain
in the actual tree consumer. Native sibling creation/recycling avoids quadratic
construction and unbounded sibling recursion. Native intern entries align their
pointer links to the platform pointer width; the Windows body remains unchanged.

This initial native tree dialect bounds each source/body to 8 MiB, depth to 256
and nodes to 65,536. It rejects embedded NUL and `<!DOCTYPE` before entering the
old parser; UTF-16/32 and DTD-bearing inputs are visibly unavailable. No external
entity resolver is installed. These bounds are separate from the signature
scanner, whose raw tag rules must preserve original signed content ordering.
`FrameXML_LoadXML` feeds unchanged bytes to an optional MD5 context before parsing
and reports read/parse failures through the original status boundary.

The 276-byte `.toc.sig` contains a 16-byte content digest, `NGIS` marker and
2048-bit little-endian RSA signature. The signed SHA1 payload is that digest
followed by the uppercase signature basename. The native application selects the
original `CGlueMgr::InterfaceKey`, distinct from the MPQ signaturefile key, and
reuses the qualified public RSA primitive with exponent 65537. Test-only explicit
keys never change that application trust anchor. Signature bypass is unavailable
in this target; original-key MPQ authentication and registered MD5 policy remain.

Content MD5 follows original depth-first TOC/XML traversal, including repeated
references, raw manifest tag scanning and the optional final raw Bindings file.
It does not substitute a semantic XML walk for the original scanner. Native bounds
reject root escape, cycles, more than 64 active references, 16,384 files or 256 MiB
of digest input. Missing files/extra input and incomplete traversal cannot produce
an authenticated result even if an authenticated digest describes only the
available prefix. Results retain original values: 0 missing/unavailable sidecar,
1 invalid envelope/RSA, 2 changed/incomplete content, 3 authenticated content.

[M3 XML Actions](../../.github/workflows/m3-xml.yml) builds Linux release/strict
ASan/UBSan/leak, actual ARM64 macOS and Windows native fixtures. It audits the
explicit source graph and retains binaries, CTest detail, hashes and Lua/Expat
notices. The separate Windows baseline remains the full-client compilation check.
All compilation runs in Actions. Qualification identities/results will be recorded
after the checks complete.

`NativeXMLCheck` exercises authored independent RSA/digest vectors, repeated and
relative references, filename binding, corruption, missing children/extra input,
cycles, owner affinity, mixed-content offsets, tree/user-data recycling, parser
recovery and wide sibling lists. Its optional original-data mode authenticates
the retail signaturefile, verifies original GlueXML and FrameXML sidecars/content
and parses original AccountLogin XML, without executing that UI or its Lua.

After downloading and verifying the selected Actions package, run without local
compilation:

```sh
NativeXMLCheck --original-data /mnt/games/3.3.5aClean/Data \
  /tmp/wowdecomp-xml-settings /tmp/wowdecomp-xml-cache
```

Use separate writable roots for strict sanitizer runs, with leak detection and
halt-on-error enabled. Original data stays read-only and outside Git/hosted CI.

Frame factories/layout/input, file-backed image/async Services and original
GlueXML startup/rendering remain the next integration work. Authenticating an
interface and parsing a tree does not establish login rendering or execution.
Networking/authentication and observed GrimGuzzler gameplay follow; M3 stays open.
Server identity discrepancies and the user's interactive sign-in preference remain.
