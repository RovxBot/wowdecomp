# M1 runtime foundation design

The portable core is a separate native ABI selected by the target-scoped
`WOWDECOMP_PORTABLE_RUNTIME` definition. The existing Windows client continues to
use its original implementations and layouts. Do not link objects compiled with
these two runtime ABIs into the same client. Neither ABI is a disk/network format.

## Ownership and startup

`WowCoreServices` implements the existing core entry points through Os, Storm and
BlizzardCore. `NativeCore` uses ordinary CRT startup, initializes Storm logging and
memory, starts Os timing, uses an OS CSPRNG, stops workers, and destroys the core
services. It repeats this lifecycle three times. A global constructor allocates
and logs before main; its destructor logs and frees after runtime shutdown.

SMem records rounded capacity and backing-allocation ownership in an aligned
header. Zero-byte allocations return a freeable minimum block. Realloc preserves
bytes up to the old rounded capacity, zeros newly exposed bytes when requested,
keeps the old allocation on failure, honors NOREALLOC/REALLOC_NULL, and frees on a
zero-size resize. Size overflow follows the same fatal/recoverable error policy
as allocation failure. Standard, array, sized, aligned and nothrow new/delete all
use that allocator. BlizzardCore's metadata allocation operators retain native
`size_t` rather than truncating it to 32 bits. No allocator switch occurs after
static construction, and shutdown never invalidates a live caller-owned block.
Native rounded capacity is deterministic; it does not expose CRT-specific heap
slack from `_msize`.

Logging serializes writes/flush/close, supports immediate/lazy file opening,
append and explicit NOFILE logs, and reports file errors through thread-local
last-error state. Its lock remains valid through late static destructors. Native
text output uses ordinary newline-terminated records; Windows crash/UI formatting
and its SEH reporter remain separate. Fatal errors print a diagnostic and terminate
rather than returning success. The process-lifetime logger lock is intentionally
retained; individual files and log records are closed/freed at shutdown.

The native executable runs core work on an explicit 8 MiB OS thread stack and
checks its actual size. It sets the platform default floating-point environment
and round-to-nearest. It does not transplant the Windows CRT entry, SEH or fiber.
The process main thread remains available for the later SDL lifecycle. This
validates core initialization/shutdown, not CommonMain, a window or gameplay.
Render/animation/SIMD floating-point parity is still a later validation task.

## Synchronization and thread lifetime

All portable waitable objects share a mutex/condition-variable registry so wait-all
can inspect and acquire the entire set atomically. It does not consume a semaphore
or auto-reset event when another member is unavailable. Wait-any selects the lowest
available index. Manual-reset events retain their signal, auto-reset events consume
one signal, semaphores are bounded, and mutexes are recursive with owner-checked
release. Thread exit marks owned mutexes abandoned. Return values retain Windows
OBJECT_0/ABANDONED_0/TIMEOUT/FAILED values. Storm's null filtering retains its compacted
index convention. Zero counts, duplicate objects and oversized wait sets fail.
Timeouts use a steady absolute deadline, including across spurious wakeups.

References held by a pending wait keep its state alive. A close observed during an
already-started wait reports failure; callers still must preserve the containing
C++ object's lifetime and avoid racing entry with object destruction. Native state
retention does not authorize callers to destroy an SSyncObject while invoking its
methods. Cancellation/shutdown use explicit events; closing a thread handle does
not cancel its callback.

Threads retain their completion state independently of handles. A thread-local
exit guard is established before the user callback. It runs registered TLS
destructors and completion callbacks, marks abandoned mutexes, then signals thread
completion. BlizzardCore's worker reference is retained through TLS destruction;
closing the caller's reference early detaches ownership without freeing live state.
Engine thread IDs/names are process-local identities, not kernel handles/IDs.

TLS keys carry generations, so freeing/reusing a slot does not expose another
thread's stale value. Values are isolated per thread. Destructors run after clearing
the value and can re-register it for up to four passes. BlizzardCore slot publication
and ConstructOnce construction are synchronized. Atomic counters use sequentially
consistent native atomics, including signed wraparound.

The wait semantics were checked against the
[Windows wait contract](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitformultipleobjects).
Advanced process/pool services are outside these core wait objects.

## Time, randomness and platform boundaries

Os clocks use steady-clock nanoseconds/milliseconds and retain uint32 millisecond
wrap at the engine API. Timing-mode changes/shutdown are entry-thread operations
performed with readers stopped. Calendar timestamps stay nanoseconds since
2000-01-01 UTC; conversion uses scalar Gregorian arithmetic, including negative
fractions, leap days, invalid dates and saturation. They do not use native `long`
for serialized time or depend on Windows time32/libc date-range restrictions.

Linux entropy uses getrandom with partial-read/EINTR handling. macOS uses bounded
getentropy requests (declared in
[Apple's sys/random.h](https://github.com/apple-oss-distributions/xnu/blob/main/bsd/sys/random.h)).
The Windows core comparison uses BCryptGenRandom. Failures terminate with a visible
diagnostic; none fall back to timestamps, hashes or uninitialized memory. The
existing full Windows client's random implementation is retained unchanged.

Windows types/headers and calling conventions remain in the legacy Windows ABI or
platform implementation files. Native interfaces use explicit integers, native
pointer widths, opaque synchronization state and native alignment. No Win32
packing, WIN32-on-POSIX defines, x86 instructions or donor addresses are introduced.
The bundled dependencies retain their existing source/library pins; this work adds
only C++17, platform thread APIs and platform CSPRNG APIs.

## M2 nearby model allocation design

The existing M2 representation is unchanged: every relative target must remain
within signed 32-bit displacement of its anchor, and buffers must outlive all
arrays referring to them. The Windows OsAllocNear/OsFreeNear path is untouched.

Before enabling native M2 loading, reserve an owner-scoped virtual-memory arena
near the model anchor, retain the reservation for the model's lifetime, and
suballocate aligned payloads within it. A mapping hint is not a guarantee: verify
the actual returned mapping, every allocation's full extent and every encoded
displacement before storing an offset. Linux can investigate non-destructive
MAP_FIXED_NOREPLACE candidate reservations; macOS requires non-destructive mapping
hints plus range verification or Mach reservation. Never replace an occupied
mapping, overwrite unrelated memory or silently fall back to arbitrary malloc.
Reject an unsatisfied range and propagate the model-load failure. Separate mapped
reservation ownership from payload/suballocation ownership during unload.

M2 must implement and fixture-test this design before model loading is enabled.
This M1 core does not advertise OsAllocNear or load model data.

## Explicit scope limits

Available here: core allocation, text/error logging, monotonic and calendar time,
CSPRNG, atomics, recursive locks, read/write locks, events/semaphores, basic Storm
and BlizzardCore threads, TLS and core initialization/shutdown.

Thread priority changes return unavailable, named cross-process mutexes return an
invalid handle, and suspended/nonzero-flag thread creation fails. These features
are not silently emulated. Legacy BlizzardCore thread pools/Using registrations,
process launching, SEH/crash UI, allocator diagnostic/report interfaces and the
remaining Windows-dependent headers are not exposed by the native core target.
Their callers must be adapted as those modules enter native builds; they are not
linked to placeholder success functions. Files/archive async I/O, networking,
window/input, rendering, audio and peripheral services remain in later milestones.
The full-client platform gate stays in place.
