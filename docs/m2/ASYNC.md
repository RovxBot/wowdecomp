# M2 native asynchronous file service

Implementation `3b950a4` passed all four hosted async configurations in
[run 38995387429](https://github.com/RovxBot/wowdecomp/actions/runs/38995387429),
including Linux ASan/UBSan and actual macOS ARM64 execution.
[PR #7](https://github.com/RovxBot/wowdecomp/pull/7) merged as
`b599a23b9d0f1acf07d39ebf88dd0ffd06dacd26`; the full Windows reference passed
[run 38995692248](https://github.com/RovxBot/wowdecomp/actions/runs/38995692248).
The queue is integrated into the [SDL3 shell](SHELL.md), with current evidence in
the [M2 handoff](HANDOFF.md). This is a
native Services boundary, not a binary-compatible replacement for CAsyncObject.
Legacy callers remain unchanged until their modules enter the native build.

The original Services queue sorts lower numerical priorities first, preserves
FIFO order within ordinary equal-priority requests, and promotes urgent requests
before equal priorities. Its worker reads outside the queue lock and its poll
handler delivers callbacks on the event thread. Cancellation cannot free a current
read: it replaces its post-load action with cleanup and retains its buffers until
the worker finishes. These are the native service's reference semantics.

The native queue owns requests, file/package references and result bytes through
worker completion and exactly one deferred callback. A retained ticket keeps its
terminal identity after queue shutdown. Operations never invoke user callbacks
under the queue lock. The constructor thread pumps completion, waits and shutdown;
foreign/reentrant pumps and shutdown inside a callback are rejected. Destruction
on the wrong thread or inside its own callback terminates rather than freeing a
live worker. Ordinary callback exceptions are reported after ready callbacks have
been drained and their requests marked terminal.

Unlike the legacy untyped callback/free-list API, native cancellation has one
explicit terminal Cancelled callback for queued, reading or ready requests when
cancellation wins before delivery. A future legacy adapter must translate its
immediate-free/deferred-cleanup return contract; this service does not silently
pretend the layouts or callback signatures match. Requests are not recycled.

One worker uses the M1 thread provider with an explicit verified 8 MiB stack and
default floating-point environment. Cancellation of a current regular-file read
is cooperative; package reads retain archive ownership until that bounded member
read finishes. Host reads check cancellation between 1 MiB chunks and report actual
short EOF counts. No required networking, downloader or graphics service is stubbed.
Shutdown stops admission, cancels outstanding requests, joins the worker and drains
callbacks before freeing state. A kernel-blocked filesystem read cannot be forcibly
aborted by this API; there is no unsafe thread kill or close-while-reading shortcut.

Fixtures exercise queued/current cancellation, priority/urgent/FIFO ordering,
owner-thread callbacks, bounded waits, callback submissions/exceptions, short EOF
reads, visible host errors, concurrent producers and repeated shutdown. Synthetic
core/file/package/decoder checks also run; no game data enters Actions.
