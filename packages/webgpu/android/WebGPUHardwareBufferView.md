# WebGPUHardwareBufferView: a "normal RN view" WebGPU canvas on Android

## Why

Android offered two on-screen canvas backends, each with a real limitation:

- **`WebGPUSurfaceView`** (opaque path). A dedicated SurfaceFlinger layer. Cheap and overlay-capable, but it is not view content: it hole-punches the window and fights RN parent transforms, clipping, rounded corners, and z-ordering with sibling views.
- **`WebGPUTextureView`** (transparent path). Real view content, but it routes GPU output through a `SurfaceTexture` (an external GL texture) into HWUI. That adds GL interop, an extra copy, and a frame of latency, and it stalls during some animations.

`WebGPUHardwareBufferView` is a third backend that aims to be strictly better than `TextureView` for the transparent case: GPU output lands in an `AHardwareBuffer`, drawn inline by HWUI via `Bitmap.wrapHardwareBuffer` + `Canvas.drawBitmap`. HWUI imports the buffer as a Skia texture and samples it zero-copy. The result is a plain `View`: any parent transform, clip, alpha, z-order, or animation applies, with no GL interop and no extra copy.

It is wired into `WebGPUView.updateView` as an opt-in: `android={{ surfaceType: "HardwareBufferView" }}` selects it on API 29+ (`Q`) and falls back to a `TextureView` below. With `android.surfaceType` left on `auto`, a non-opaque canvas (`opaque={false}`) uses `WebGPUBlitTextureView`, the canvas-mode TextureView described in `docs/Presentation.md`. Two things were open before it could become the default for the transparent case. The runtime fallback now exists (see [Runtime fallback](#runtime-fallback)). The latency comparison against `TextureView` has been measured on one device (see [Status](#status)) and does not show the advantage the design hoped for, so the default has not changed.

## The two hard problems

Everything in the design exists to solve buffer synchronization and resize.

### Acquire (rigorous)

WebGPU renders into self-owned `AHardwareBuffer`s that Dawn imports as `SharedTextureMemory`. On present, Dawn's `EndAccess` produces a real render-complete fence. A dedicated native waiter thread blocks on that fence before a frame is published as "ready", so HWUI only ever samples a fully-rendered buffer.

Note: `Bitmap.wrapHardwareBuffer` / `Canvas.drawBitmap` take no fence, and there is no public HWUI API to inject a wait-fence into an inline bitmap draw. So the only way to consume the acquire fence on the inline path is a CPU wait, done off the UI thread on the waiter. That is exactly what this view does.

### Release (heuristic)

HWUI exposes no "GPU finished reading this bitmap" fence. So a displayed buffer cannot be recycled rigorously. Instead a **held-ring of 2** displayed frames is kept before a slot returns to the pool, giving HWUI time to finish sampling. The pool is sized to absorb this (5 slots). This is the one place the design is best-effort rather than fenced, and it is inherent to inline view content (`TextureView` only gets this right because the framework wired internal fence plumbing into its `SurfaceTexture` consumer that app code cannot reach).

## Presentation model

`SurfaceInfo` (in `cpp/rnwgpu/SurfaceRegistry.h`) has a third mode beside the on-screen `wgpu::Surface` swapchain and the offscreen texture: the **AHB pool**. The pool lives in its own class, `HardwareBufferPresenter` (in `cpp/rnwgpu/HardwareBufferPresenter.h`), which `SurfaceInfo` owns. `SurfaceInfo` only decides when pool mode is active and forwards the producer side to it through the `FramePresenter` interface (`cpp/rnwgpu/FramePresenter.h`), which the copy-based `BlitPresenter` implements too; the view consumes finished frames from the presenter directly.

A pool is one generation of `kPoolSize` (5) slots. Each slot holds an `AHardwareBuffer*`, a `wgpu::SharedTextureMemory`, a `wgpu::Texture`, and a state:

```
Free -> Rendering -> Presented -> Ready -> Displayed -> (held) -> Free
```

Generations are `shared_ptr`-counted so an in-flight render, present, or ready slot keeps the whole generation alive across a resize. The destructor frees every `AHardwareBuffer` and tears down the Dawn imports. The Java side keeps its own ref (via `wrapHardwareBuffer`) for anything still on screen, so a generation can be freed natively without disturbing a frame still being drawn.

### Sizing (the part that matters most)

The pool is **allocated natively and sized from the canvas drawing buffer**, not from the view. `SurfaceInfo::getCurrentTexture` calls `HardwareBufferPresenter::resize` with the drawing buffer size (the JS `canvas.width`/`canvas.height`, which `GPUCanvasContext::getCurrentTexture` keeps in sync through `reconfigure()`) before acquiring a slot, reallocating the pool when that size changes. This mirrors exactly how the swapchain reconfigures its surface to `canvas.width`.

Why it must be this way: apps create their other attachments (for example a depth texture) at `canvas.width`/`canvas.height`. WebGPU requires every attachment in a render pass to have identical dimensions. If the canvas color texture is sized differently (even by 1px), every `beginRenderPass` fails validation, the render loop throws, and the screen is blank. `canvas.width` is derived in JS as `clientWidth * PixelRatio`, while a view's pixel size is `round(dp * density)`; on non-integer-density devices these differ. Sizing the pool from `canvas.width` keeps the canvas texture aligned with the app's attachments on every device.

The Java side only reports the dp client size (`nEnablePool` / `nSetClientSize`), which feeds `getSize()` and therefore `canvas.clientWidth/Height`. It does not allocate buffers and does not pick the render resolution.

## Runtime fallback

The pool cannot serve every canvas. It needs:

- a device with the `SharedTextureMemoryAHardwareBuffer` feature,
- an `rgba8unorm` canvas (the buffers are `R8G8B8A8_UNORM`),
- no `viewFormats`,
- only usages the imported buffers support,
- a successful `AHardwareBuffer_allocate` and Dawn import.

`HardwareBufferPresenter::resize` checks this the first time a frame is acquired after the view attached or the canvas was configured. When the pool is unusable it drops whatever it had, returns no texture (so that frame is rendered offscreen), and invokes the unsupported callback. That reaches `WebGPUHardwareBufferView.onNativeUnsupported`, which asks `WebGPUView` to replace the view with a `WebGPUBlitTextureView`.

`WebGPUBlitTextureView` presents with a copy. WebGPU renders into two plain textures owned by `BlitPresenter` (`cpp/rnwgpu/BlitPresenter.h`); `present()` swaps them and wakes the view, and the UI thread copies the front one onto the TextureView's swapchain and presents it. The swapchain is only used from the UI thread, and it is never configured with the canvas's view formats or extra usages, so those keep working. The frame rendered offscreen during the switch is handed to the new presenter, so a canvas that renders once still shows up.

The view presents behind the window's pending draw and waits for `onSurfaceTextureUpdated` before presenting the next frame (`FrameScheduler.java`, shared with `WebGPUBlitSurfaceView` through `BlitPresenterClient.java`; the pacing is explained in `docs/Presentation.md`). A frame that finishes in the meantime replaces the one waiting. Nothing paces the rendering thread, so when the GPU is the bottleneck the app renders frames that are never shown (see [Status](#status)).

The replacement is sticky for the `WebGPUView`: it goes back to the hardware buffer view only when the `surfaceType` prop changes. Below API 29 the request still resolves to the plain `WebGPUTextureView`, which renders straight to its swapchain.

## Threading model

- **Producer (JS render thread).** `getCurrentTexture` returns the in-flight slot again if the frame has not been presented yet (per the WebGPU spec), otherwise pulls the next `Free` slot (blocking with backpressure, bounded to 1s so a wedged fence skips the frame instead of hanging JS), `BeginAccess`, returns its texture. `present` does `EndAccess`, collects the fence(s), queues `(slot, fences)` for the waiter, and returns immediately so the thread can race ahead to another slot.
- **Waiter thread (native).** Pops a queued frame, blocks on each fence by exporting it to a sync-fd and `poll(POLLIN)` on it (bounded slices so teardown cannot hang), then publishes the slot as the latest "ready" and wakes the consumer through the frame-ready callback (a JNI call to `WebGPUHardwareBufferView.onNativeFrameReady`, made outside the pool lock through a weak reference to the view; the thread attaches to the JVM for the duration of the call). A superseded, still-unclaimed ready frame returns to `Free`.
- **Consumer (UI thread).** `onNativeFrameReady` coalesces wake-ups into a single `consume()` posted to the main looper. It calls `nPollReady` for the latest ready `(generation, slot)`, fetches that buffer via `nGetHardwareBuffer` (which returns a `HardwareBuffer` via `AHardwareBuffer_toHardwareBuffer`), wraps it in a `Bitmap` (cached per token), and `invalidate()`s so HWUI draws it at the next vsync. The held-ring releases old slots back to the pool via `nReleaseSlot`. There is no per-vsync polling: an idle canvas costs nothing on the UI thread.

Why `poll()` instead of `sync_wait`: Android sync fences become readable (`POLLIN`) when signaled, and `poll` uses only libc, avoiding any `libsync` linkage concern.

Slot accounting at steady state: up to 2 held (display + release safety) + 1 rendering + 1 in the waiter queue + 1 ready = 5, hence `kPoolSize = 5`.

### Fence ownership (fdsan)

Each exported sync-fd is owned by its `wgpu::SharedFence` and closed when the fence is destroyed at the end of the waiter iteration. The code never `dup`s or closes the fd itself, which avoids the double-close that trips Android's fdsan.

## onDraw and resize

`onDraw` always scales the last good frame to the current bounds (`src = bitmap bounds`, `dst = view bounds`, `FILTER_BITMAP`). So a buffer that momentarily lags the view size mid-resize is shown stretched rather than blank.

On resize the native pool reallocates to the new canvas size (a new generation), while the previous generation is kept alive for the cross-fade. The consumer keeps drawing the last frame scaled until the first new-size frame lands. Native keeps the current and previous generation (`_pools`); the Java side recycles cached `Bitmap`s for generations older than that once they leave the held-ring.

## Lifecycle

- **Attach / size change.** `onSizeChanged` and `onAttachedToWindow` call `nEnablePool` (first time, which also registers the frame-ready callback) or `nSetClientSize` (subsequently) with the dp size. Attach also posts one `consume()` to pick up a frame that became ready while detached.
- **Detach.** The frame-ready callback is unregistered, `nSwitchToOffscreen` flips the context to an offscreen texture so JS keeps rendering safely (it does not block in `getCurrentTexture`), any pending `consume()` is cancelled, and all cached `Bitmap`s are recycled. A frame caught between `getCurrentTexture` and `present` gets its `EndAccess` and is handed to the waiter, so its fences are drained before the retired generation is freed; the waiter discards frames that finish after teardown instead of publishing them. The `SurfaceInfo` is intentionally not removed from the registry, so a temporary detach/re-attach (for example scrolling off screen) keeps working on the same context.
- **Device going away.** `configure` with another device, `unconfigure` and `releaseForDevice` (called right before `device.destroy()`) release every Dawn object of the previous device on the calling thread before returning. That includes frames still with the waiter: queued ones are dropped, and the one it is waiting on is taken back (the waiter gives up its fence wait within one poll slice). Left to the waiter, those textures would be released from its thread while the device is being destroyed, which aborts inside Dawn.
- **Drop.** When RN drops the view for good, `WebGPUViewManager.onDropViewInstance` removes the `SurfaceInfo` from the registry. The JS context keeps its own reference; once JS releases it, the destructor stops the fence waiter thread and frees the remaining GPU resources.

## Files

- `cpp/rnwgpu/FramePresenter.h`: the interface `SurfaceInfo` drives a presentation backend through from the rendering thread (`configure`, `unconfigure`, `releaseForDevice`, `resize`, `getCurrentTexture`, `present`).
- `cpp/rnwgpu/BlitPresenter.h`: the copy-based backend used as the fallback. Portable C++, no Android-specific code besides logging.
- `cpp/rnwgpu/HardwareBufferPresenter.h`: the pool itself. Allocation/import, `BeginAccess`/`EndAccess`, the waiter thread and fence wait, the slot state machine, generations, the producer API (`configure`, `resize`, `getCurrentTexture`, `present`) and the consumer API (`pollReady`, `bufferForDisplay`, `releaseSlot`, `setFrameReadyCallback`).
- `cpp/rnwgpu/SurfaceRegistry.h`: the presenter modes in `SurfaceInfo`. It owns both presenters, turns them on and off (`enablePool`, `enableBlit`, and `detach` on the way out), reports the dp client size, routes `getCurrentTexture` / `presentFrame` to whichever is active, and hands the offscreen frame over to the blit presenter and back.
- `cpp/rnwgpu/api/GPUCanvasContext.cpp`: keeps the drawing buffer size in sync with `canvas.width`/`canvas.height`, which is what the pool is sized from.
- `android/cpp/cpp-adapter.cpp`: JNI: `nEnablePool`, `nSetClientSize`, `nGetHardwareBuffer`, `nPollReady`, `nReleaseSlot`, `nSwitchToOffscreen`, the `BlitPresenterClient` entry points (`nAttach`, `nSetClientSize`, `nPresentFrame`, `nDetach`), and `JavaViewCallback` (native-to-view notifications: frame ready, pool unsupported).
- `android/src/main/java/com/webgpu/WebGPUHardwareBufferView.java`: the view: enable pool mode, wake-up-driven consume, per-token Bitmap cache, held-ring, scaled `onDraw`, lifecycle. It is a pure consumer (about 250 lines); everything about buffers, fences and the swapchain lives in C++.
- `android/src/main/java/com/webgpu/WebGPUBlitTextureView.java`: the fallback view, and the canvas-mode default for a non-opaque canvas. A TextureView whose surface is only used from the UI thread; `BlitPresenterClient.java` and `FrameScheduler.java` hold the wake-up and the pacing.
- `android/src/main/java/com/webgpu/WebGPUView.java`: selects `WebGPUHardwareBufferView` for `surfaceType: "HardwareBufferView"` on API Q+ in canvas mode (opt-in; `auto` stays on `WebGPUBlitTextureView` for the transparent path), and replaces it with `WebGPUBlitTextureView` when the pool reports it is unusable.
- `android/src/main/java/com/webgpu/WebGPUAPI.java`: adds `getContextId()`.

## Prerequisites that already hold

- Device features `SharedTextureMemoryAHardwareBuffer` and `SharedFenceSyncFD` are auto-injected into every device when the adapter supports them (`GPUAdapter::requestDevice`), so the canvas device supports the import and the sync-fd fence export with no app change.
- Android's preferred canvas format is `RGBA8Unorm` (`GPU::getPreferredCanvasFormat`), which matches the AHB's `R8G8B8A8_UNORM` format, so the pool texture matches an app that uses `getPreferredCanvasFormat()`. A different configured format, view formats, or a usage the imported memory does not support make the view fall back to the copy presenter (see [Runtime fallback](#runtime-fallback)).

## Building and testing

The example app resolves `react-native-webgpu` from `node_modules/react-native-webgpu`, which `yarn install` symlinks to `packages/webgpu`, so a gradle build picks up edits in `packages/webgpu` directly.

Compile-check native and Java for one ABI:

```
apps/example/android/gradlew -p apps/example/android \
  :react-native-webgpu:externalNativeBuildDebug \
  :react-native-webgpu:compileDebugJavaWithJavac \
  -PreactNativeArchitectures=arm64-v8a
```

Install and run on device:

```
cd apps/example/android && ./gradlew :app:installDebug
```

Logcat tag for the pool: `WebGPUHardwareBufferView`. It only logs failures and anomalies (allocation/import failure, a device or canvas configuration the pool cannot serve, a free-slot timeout), never per-frame or per-resize. The copy presenter logs under `WebGPUBlitPresenter`, only when the surface cannot take the canvas format.

## Status

Verified on device:

- `Cube.tsx` (an `opaque={false}` canvas) renders the rotating cube over RN content, confirming allocation, import, fenced acquire, present, and inline draw end to end.
- The `Resize` example renders correctly through resizing, confirming the cross-fade path: native pool reallocation to the new canvas size, the kept previous generation, and the scaled last-frame `onDraw`.

- The `Android View Props` diagnostic switches one canvas between every backing view, including the runtime fallback (its format toggle asks for `rgba16float`, which the pool cannot back). The `Device Destroy Before Detach` diagnostic destroys the device with the pool, or the copy presenter, still live.

### Latency against TextureView

Measured once, on a Pixel 8 at 120 Hz, with temporary instrumentation: a full-screen fragment shader whose loop count sets the GPU cost, rendered from the JS thread on `requestAnimationFrame`. "Pickup" is the time from `context.present()` to the UI draw pass that takes the frame (`onDraw` here, `onSurfaceTextureUpdated` for the two TextureViews). The HWUI columns come from `dumpsys gfxinfo` and cover the whole window. One 7 s window per cell, so treat small differences as noise.

| Loop count | View | Shown fps | Rendered fps | Pickup p50 / p90 (ms) | HWUI janky | HWUI frame p50 / p90 / p99 (ms) |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | HardwareBufferView | 116 | 118 | 7.3 / 15.5 | 0% | 6 / 7 / 9 |
| 0 | TextureView | 112 | 114 | 6.5 / 7.5 | 0% | 5 / 6 / 8 |
| 0 | copy | 119 | 119 | 6.8 / 7.7 | 0.1% | 5 / 6 / 7 |
| 25 | HardwareBufferView | 114 | 115 | 14.9 / 16.4 | 0.6% | 5 / 10 / 19 |
| 25 | TextureView | 113 | 112 | 6.5 / 7.9 | 1.7% | 10 / 13 / 20 |
| 25 | copy | 92 | 104 | 7.5 / 15.0 | 0.7% | 9 / 13 / 18 |
| 80 | HardwareBufferView | 58 | 59 | 22.1 / 23.4 | 0% | 6 / 7 / 8 |
| 80 | TextureView | 57 | 57 | 5.2 / 6.9 | 2.5% | 11 / 14 / 27 |
| 80 | copy | 59 | 78 | 5.9 / 7.4 | 7.1% | 10 / 24 / 65 |
| 120 | HardwareBufferView | 56 | 56 | 55.5 / 60.3 | 9.1% | 19 / 85 / 109 |
| 120 | TextureView | 46 | 48 | 5.4 / 7.7 | 42.8% | 17 / 34 / 48 |
| 120 | copy | 55 | 53 | 13.5 / 18.8 | 35.5% | 25 / 73 / 85 |

What the pickup column does not show: a TextureView buffer can be picked up before the GPU has finished it, and HWUI's render thread then waits for it. That wait lands in the HWUI frame time instead. A hardware buffer is only picked up once it is finished. For the TextureView the frame also had to be matched to the latest `present()`, because its queue replaces buffers that were not latched yet, so its pickup is a lower bound.

What the numbers say:

- With cheap frames the three are equivalent at the median: the frame is picked up at the next display refresh. The hardware buffer view has a tail, about one frame in ten is picked up a refresh later.
- As soon as a frame costs a few milliseconds of GPU time, the hardware buffer view is one refresh behind for most frames (15 ms against 6.5 ms at a loop count of 25, both still near 115 fps). This is the cost anticipated above: the fence has to signal before the UI thread is even told about the frame.
- When the GPU is the bottleneck, the hardware buffer view falls about three frames behind (55 ms at 56 fps), because up to three frames are in flight in the pool. The TextureView stays one frame behind, but the whole window pays for it: 43% of its HWUI frames are janky against 9%, since HWUI waits on the buffer.
- The copy presenter tracks the TextureView on latency. Under load it renders more frames than it shows (78 against 59 at a loop count of 80): the wasted work comes from the rendering thread not being paced.

So the hardware buffer view is not a latency win. What it buys is view semantics without the SurfaceTexture, and a UI that stays smooth when the canvas is GPU-bound. Two follow-ups would narrow the gap: fewer frames in flight when the pool is the bottleneck, and pacing the rendering thread in the copy presenter.

### Churn

A stress run kept creating canvases for 3.5 minutes (about 200 of them, each alive for 0.7 s), with `device.destroy()` on every unmount, a drawing buffer resize every 12 frames (a new pool generation each time), and the app sent to the background every 40 s; first on the pool, then on the copy presenter. No fdsan report, and the process was back at its starting file descriptor count afterwards.

The first run of that stress crashed inside Dawn (`pthread_mutex_destroy called on a destroyed mutex`, from the waiter thread releasing a pool generation while the device was being destroyed). Releasing the device's objects synchronously fixed it, see "Device going away" under [Lifecycle](#lifecycle).

Not verified: a release build (`@Keep` on the methods called from JNI), and any device other than the Pixel 8.
