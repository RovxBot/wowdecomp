# SDL display-size delivery to original Frames

This native host prerequisite connects the existing SDL `Window::Pump` and Os
queue to the qualified original Frame root. `DispatchWindowEvent` handles size
events after the host dequeues them; it returns false for other events so the host
can deliver them to their own consumers. It never drains or owns the queue.
Keyboard, committed text and focus now use the separate
[input adapter](FRAME_KEYBOARD.md). Mouse, IME and animation delivery remain open.

The dispatcher queries the live logical and drawable dimensions, avoiding stale
queued sizes. It refreshes Gx window/viewport state, sets the original coordinate
aspect ratio, then invokes `CSimpleTop::OnDisplaySizeChanged`. Gxu invalidates font
dimensions/glyph caches; Frames notify their regions, and the original display
callback receives a `CSizeEvent`. Pending layout finishes before subsequent root
batching. Logical dimensions remain the Gxu/layout metrics; drawable pixels remain
the Gx target extent. Zero dimensions defer mutation until a later size event.
Bound offscreen targets retain their ownership through the existing Gx size path.
The host must establish the original coordinate aspect before constructing fonts
and layout, as `CGlueMgr::Resume` does. The probe now performs that startup step;
literal default DDC scales differ from recomputed 4:3 by one float ULP and must not
be mixed with the resize path's computed baseline.

Delivery belongs to the current context and root owner outside update, collection,
draw and resize callbacks. The screen traversal now holds a resize guard, including
the external display callback: hierarchy mutation, destruction and reentry reject.
Callbacks may request the existing deferred destruction queue. Exceptions propagate
and unwind the guard; the caller must complete a subsequent delivery/update before
rendering. No callback or window pointer is retained by the dispatcher. Close the
root and its consumers before font/renderer/Gxu/layout/XML/VM/device/context owners.

`NativeFrameHierarchyCheck --window-events` adds three real programmatic SDL window
resizes (800x600, 900x600, 640x480) in each of three complete owner cycles. The path
is `SDL_SetWindowSize`/`SDL_SyncWindow`, `Window::Pump`, `OsQueueGet`, dispatcher,
Gxu/root/region callbacks, pending layout and rendered glyph readback. It checks
logical/drawable sizes, aspect/root extent, font pixel-size changes, repeat pixels
and exact restoration. Separate authored stale-size, callback failure/reentry and
foreign-thread checks do not count as physical input. SDL synchronization follows
the pinned version's [window API contract](https://wiki.libsdl.org/SDL3/SDL_SyncWindow).

All client/dependency compilation and CTest run in Actions, including strict
ASan/UBSan/leak checks and the separate full Windows x64 regression. The native
target selects original `CEvent.cpp`; original Windows function bodies remain
unchanged. No donor/dependency change or signature-policy relaxation occurs.
The selected implementation is qualified below. This is authored layout/text,
not original login.

## Qualification

[PR #51](https://github.com/RovxBot/wowdecomp/pull/51) is qualified at
`b173d38012681e61780c6d60f6dc069782220304`, merged as
`e36987aa8e58502708e29696a535e4d841a0dde9`. The
[durable record](frame-window-validation.json) retains all thirteen passing PR
workflows, 35 CTest checks per Linux release/strict ASan/UBSan/leak configuration,
and full Windows x64 compilation with signatures enabled. Hosted checkout
`ccdb724540ca225850355886b7c69b19ff564767` has the same tree as the implementation
and merge. Three archive digests, all 51 packaged executable hashes/types and
notices verified. This graphics surface is Linux only; prerequisite CPU workflows
retain their separate actual ARM64 macOS and native Windows coverage.

Downloaded exact-source release and strict sanitizer packages each passed three
authored and three original-data window cycles, plus authored/original hierarchy,
region, retained batch, GlueFonts, image and Gxu cycles. All eighteen executions
exited 0 with empty stderr and zero workers. The target/transform/buffer/texture
fixtures also passed three cycles per configuration on RX 7900 XTX / Mesa 26.2.5 /
SDL X11. Original-key authentication observed 28,948 signaturefile records and
retained registered-member MD5 policy. Each original-data window/hierarchy cycle
authenticated GlueXML before and after constructing eighteen original GlueFonts.

Each window cycle observed 800x600, 900x600 and 640x480 logical/drawable dimensions.
The selected original font changed from six raster pixels to eight and back to
six; rendered ink changed from 131 pixels to 207 and back to 131. The authored
20-pixel font changed to 25 and back to 20. Repeat/restoration pixels match exactly.
Release/sanitizer original window and hierarchy captures match SHA-256
`55b0e2c731d888293851e66a21b2b7eccaa29405097fb4b6b1e65588f60c93cf`;
the 640x480 release capture was inspected. Matching original startup coordinate
initialization moves the historical hierarchy image up one pixel (162 pixel
positions differ), with unchanged glyph coverage. Preserve PR #49's original
capture identity. Loading-bar regression capture hashes remain unchanged.

Earlier hosted release runs passed 34/35 checks. The initial font hit the original
32-pixel raster cap; window mode now uses an uncapped authored 20-pixel font.
Subsequent release and strict runs passed the new resize/restoration sequence but
failed the later parent scale-restoration comparison. The diagnostic identifies
fixture line 283: startup had used literal DDC defaults before the first resize.
The host now calls the original coordinate setter before fonts/layout, matching
`CGlueMgr::Resume`. No pixel assertion or detector was relaxed. Failed and
superseded/cancelled runs remain recorded. Local Python: 54 passed, one CMake
availability skip; 269 original Windows body comparisons and whitespace passed.
No client/dependency compiled locally.

Run a manifest-verified hosted artifact without local compilation:

```sh
python3 Tools/Porting/run_native_gl.py --artifact /tmp/verified-frame-window \
  --frame-window --data /mnt/games/3.3.5aClean/Data \
  --settings /tmp/frame-window-settings --cache /tmp/frame-window-cache \
  --output /tmp/frame-window.json --snapshot /tmp/frame-window.ppm
```

Use `--sanitizers` and separate user roots for the sanitizer package. The supplied
installation stays read-only, outside Git and hosted CI. Independent high-DPI
scale changes, physical input, Windows runtime and macOS graphics require separate
observations. Pooled Lua/XML factories, texture/model/animation consumers, complete
signed ordered GlueXML and its post-creation digest gate, original login and
transport/gameplay remain open. Preserve server revision discrepancies and the
user's interactive sign-in preference; M3 remains incomplete.
