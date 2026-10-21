# SDL display-size delivery to original Frames

This native host prerequisite connects the existing SDL `Window::Pump` and Os
queue to the qualified original Frame root. `DispatchWindowEvent` handles size
events after the host dequeues them; it returns false for other events so the host
can deliver them to their own consumers. It never drains or owns the queue.
Keyboard, mouse, focus, IME and animation delivery to Frames remain open.

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
Implementation and downloaded original-data qualification will be recorded after
the hosted checks complete. This is authored layout/text, not original login.

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
