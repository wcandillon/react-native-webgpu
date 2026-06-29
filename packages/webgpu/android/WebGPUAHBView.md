# WebGPUAHBView: a "normal RN view" WebGPU canvas on Android

## Why

Android offered two on-screen canvas backends, each with a real limitation:

- **`WebGPUSurfaceView`** (opaque path). A dedicated SurfaceFlinger layer. Cheap and overlay-capable, but it is not view content: it hole-punches the window and fights RN parent transforms, clipping, rounded corners, and z-ordering with sibling views.
- **`WebGPUTextureView`** (transparent path). Real view content, but it routes GPU output through a `SurfaceTexture` (an external GL texture) into HWUI. That adds GL interop, an extra copy, and a frame of latency, and it stalls during some animations.

`WebGPUAHBView` is a third backend that aims to be strictly better than `TextureView` for the transparent case: GPU output lands in an `AHardwareBuffer`, drawn inline by HWUI via `Bitmap.wrapHardwareBuffer` + `Canvas.drawBitmap`. HWUI imports the buffer as a Skia texture and samples it zero-copy. The result is a plain `View`: any parent transform, clip, alpha, z-order, or animation applies, with no GL interop and no extra copy.

It is wired into `WebGPUView.setTransparent`: on API 29+ (`Q`) the transparent path uses `WebGPUAHBView`, otherwise it falls back to `WebGPUTextureView`. The opaque path is unchanged.

## The two hard problems

Everything in the design exists to solve buffer synchronization and resize.

### Acquire (rigorous)

WebGPU renders into self-owned `AHardwareBuffer`s that Dawn imports as `SharedTextureMemory`. On present, Dawn's `EndAccess` produces a real render-complete fence. A dedicated native waiter thread blocks on that fence before a frame is published as "ready", so HWUI only ever samples a fully-rendered buffer.

Note: `Bitmap.wrapHardwareBuffer` / `Canvas.drawBitmap` take no fence, and there is no public HWUI API to inject a wait-fence into an inline bitmap draw. So the only way to consume the acquire fence on the inline path is a CPU wait, done off the UI thread on the waiter. That is exactly what this view does.

### Release (heuristic)

HWUI exposes no "GPU finished reading this bitmap" fence. So a displayed buffer cannot be recycled rigorously. Instead a **held-ring of 2** displayed frames is kept before a slot returns to the pool, giving HWUI time to finish sampling. The pool is sized to absorb this (5 slots). This is the one place the design is best-effort rather than fenced, and it is inherent to inline view content (`TextureView` only gets this right because the framework wired internal fence plumbing into its `SurfaceTexture` consumer that app code cannot reach).

## Presentation model

`SurfaceInfo` (in `cpp/rnwgpu/SurfaceRegistry.h`) gained a third mode beside the on-screen `wgpu::Surface` swapchain and the offscreen texture: the **AHB pool**.

A pool is one generation of `kPoolSize` (5) slots. Each slot holds an `AHardwareBuffer*`, a `wgpu::SharedTextureMemory`, a `wgpu::Texture`, and a state:

```
Free -> Rendering -> Presented -> Ready -> Displayed -> (held) -> Free
```

Generations are `shared_ptr`-counted so an in-flight render, present, or ready slot keeps the whole generation alive across a resize. The destructor frees every `AHardwareBuffer` and tears down the Dawn imports. The Java side keeps its own ref (via `wrapHardwareBuffer`) for anything still on screen, so a generation can be freed natively without disturbing a frame still being drawn.

### Sizing (the part that matters most)

The pool is **allocated natively and sized from the canvas drawing buffer**, not from the view. `GPUCanvasContext::getCurrentTexture` calls `poolResize(_canvas->getWidth(), _canvas->getHeight())` (the JS `canvas.width`/`canvas.height`) before acquiring a slot, reallocating the pool when that size changes. This mirrors exactly how the swapchain reconfigures its surface to `canvas.width`.

Why it must be this way: apps create their other attachments (for example a depth texture) at `canvas.width`/`canvas.height`. WebGPU requires every attachment in a render pass to have identical dimensions. If the canvas color texture is sized differently (even by 1px), every `beginRenderPass` fails validation, the render loop throws, and the screen is blank. `canvas.width` is derived in JS as `clientWidth * PixelRatio`, while a view's pixel size is `round(dp * density)`; on non-integer-density devices these differ. Sizing the pool from `canvas.width` keeps the canvas texture aligned with the app's attachments on every device.

The Java side only reports the dp client size (`nEnablePool` / `nSetClientSize`), which feeds `getSize()` and therefore `canvas.clientWidth/Height`. It does not allocate buffers and does not pick the render resolution.

## Threading model

- **Producer (JS render thread).** `getCurrentTexture` pulls the next `Free` slot (blocking with backpressure if none), `BeginAccess`, returns its texture. `present` does `EndAccess`, collects the fence(s), queues `(slot, fences)` for the waiter, and returns immediately so the thread can race ahead to another slot.
- **Waiter thread (native, not JVM-attached).** Pops a queued frame, blocks on each fence by exporting it to a sync-fd and `poll(POLLIN)` on it (bounded slices so teardown cannot hang), then publishes the slot as the latest "ready". A superseded, still-unclaimed ready frame returns to `Free`.
- **Consumer (UI thread).** A `Choreographer.FrameCallback` runs each vsync: it calls `nPollReady` for the latest ready `(generation, slot)`, fetches that buffer via `nGetHardwareBuffer` (which returns a `HardwareBuffer` via `AHardwareBuffer_toHardwareBuffer`), wraps it in a `Bitmap` (cached per token), and draws it. The held-ring releases old slots back to the pool via `nReleaseSlot`.

Why `poll()` instead of `sync_wait`: Android sync fences become readable (`POLLIN`) when signaled, and `poll` uses only libc, avoiding any `libsync` linkage concern.

Slot accounting at steady state: up to 2 held (display + release safety) + 1 rendering + 1 in the waiter queue + 1 ready = 5, hence `kPoolSize = 5`.

### Fence ownership (fdsan)

Each exported sync-fd is owned by its `wgpu::SharedFence` and closed when the fence is destroyed at the end of the waiter iteration. The code never `dup`s or closes the fd itself, which avoids the double-close that trips Android's fdsan.

## onDraw and resize

`onDraw` always scales the last good frame to the current bounds (`src = bitmap bounds`, `dst = view bounds`, `FILTER_BITMAP`). So a buffer that momentarily lags the view size mid-resize is shown stretched rather than blank.

On resize the native pool reallocates to the new canvas size (a new generation), while the previous generation is kept alive for the cross-fade. The consumer keeps drawing the last frame scaled until the first new-size frame lands. Native keeps the current and previous generation (`_pools`); the Java side recycles cached `Bitmap`s for generations older than that once they leave the held-ring.

## Lifecycle

- **Attach / size change.** `onSizeChanged` and `onAttachedToWindow` call `nEnablePool` (first time) or `nSetClientSize` (subsequently) with the dp size, and start the Choreographer callback.
- **Detach.** `nSwitchToOffscreen` flips the context to an offscreen texture so JS keeps rendering safely (it does not block in `getCurrentTexture`), the Choreographer callback is removed, and all cached `Bitmap`s are recycled. The `SurfaceInfo` is intentionally not removed from the registry, so a temporary detach/re-attach (for example scrolling off screen) keeps working on the same context.

## Files

- `cpp/rnwgpu/SurfaceRegistry.h` — the AHB-pool mode in `SurfaceInfo`: allocation/import, `BeginAccess`/`EndAccess`, the waiter thread and fence wait, the slot state machine, generations, and the public pool API (`enablePool`, `setPoolClientSize`, `poolResize`, `poolPollReady`, `poolBufferForDisplay`, `poolReleaseSlot`).
- `cpp/rnwgpu/api/GPUCanvasContext.cpp` — `getCurrentTexture` drives `poolResize` in pool mode; `present` fires for pool mode (`hasSurface() || isPoolMode()`).
- `android/cpp/cpp-adapter.cpp` — JNI: `nEnablePool`, `nSetClientSize`, `nGetHardwareBuffer`, `nPollReady`, `nReleaseSlot`, `nSwitchToOffscreen`.
- `android/src/main/java/com/webgpu/WebGPUAHBView.java` — the view: enable pool mode, Choreographer-driven consume, per-token Bitmap cache, held-ring, scaled `onDraw`, lifecycle.
- `android/src/main/java/com/webgpu/WebGPUView.java` — wires the transparent path to `WebGPUAHBView` on API Q+.
- `android/src/main/java/com/webgpu/WebGPUAPI.java` — adds `getContextId()`.

## Prerequisites that already hold

- Device features `SharedTextureMemoryAHardwareBuffer` and `SharedFenceSyncFD` are auto-injected into every device when the adapter supports them (`GPUAdapter::requestDevice`), so the canvas device supports the import and the sync-fd fence export with no app change.
- Android's preferred canvas format is `RGBA8Unorm` (`GPU::getPreferredCanvasFormat`), which matches the AHB's natural `R8G8B8A8_UNORM` format, so the natural-format pool texture matches an app that uses `getPreferredCanvasFormat()`.

## Building and testing

The example app resolves `react-native-webgpu` from `node_modules/react-native-webgpu`, which is a copy, not a symlink to `packages/webgpu`. Edits in `packages/webgpu` must be copied into `node_modules/react-native-webgpu/...` before a gradle build picks them up.

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

Logcat tag for the pool: `WebGPUAHBView` (logs each `poolResize` and any allocation/import failure).

## Status

Verified on device:

- `Cube.tsx` (a `transparent` canvas) renders the rotating cube over RN content, confirming allocation, import, fenced acquire, present, and inline draw end to end.
- The `Resize` example renders correctly through resizing, confirming the cross-fade path: native pool reallocation to the new canvas size, the kept previous generation, and the scaled last-frame `onDraw`.

Not yet measured: performance versus `TextureView`, and fdsan stability under sustained churn.

The debug `RNWGPU_POOL_LOG` traces in `SurfaceRegistry.h` should be removed before shipping.
