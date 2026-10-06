# Canvas presentation

How a frame rendered into a `<Canvas>` reaches the screen on iOS and Android,
and who presents it when. The user-facing surface of all this is one prop,
`mode`, documented in `apps/docs/content/api/canvas.mdx`; this file is the
design behind it.

## Two modes

A canvas is in one of two modes, chosen with `<Canvas mode>`:

| | `"canvas"` (default) | `"swapchain"` |
| --- | --- | --- |
| Contract | The web one: `getCurrentTexture()`, submit, and the frame shows up | The same calls, but `present()` presents the native swapchain itself |
| Who presents | The native view, on the UI thread, when the platform can take a frame | The thread that rendered, right away |
| Rendering thread touches the swapchain | Never | Always |
| Extra copy per frame | One (the blit presenter) or none (the hardware buffer presenter) | None |
| Latency | Up to one display refresh more | Lowest |
| Android | `WebGPUBlitSurfaceView` (opaque) or `WebGPUBlitTextureView` (non-opaque); `WebGPUHardwareBufferView` opt-in | `WebGPUSurfaceView` or `WebGPUTextureView` |
| iOS | `MetalView` in canvas mode (a `CADisplayLink` copies frames onto its `CAMetalLayer`) | `MetalView` presenting the layer's swapchain from the rendering thread |
| macOS | Not available: always swapchain | `MetalView` |
| web | n/a (the browser presents) | n/a |

The modes differ in *who presents*, not in how the canvas composites. On
Android, `android.surfaceType` still picks between a `SurfaceView` (its own
compositor layer) and a `TextureView` (regular view content), in both modes;
`auto` takes `SurfaceView` for an opaque canvas and `TextureView` otherwise.
`HardwareBufferView` is a canvas-mode view with no swapchain, so in swapchain
mode (or below API 29) it resolves to a `TextureView`.

Canvas mode is the default because it is the browser contract and because
the native surface then never leaves the UI thread: the platform creates and
destroys surfaces there, and sharing a `wgpu::Surface` with whichever thread
renders (main JS, the Reanimated UI runtime, a worklet runtime) is what the
latching machinery in `SurfaceRegistry.h` exists to make safe. Swapchain mode
stays for the cases where the extra copy or the extra refresh of latency
matter more than that; the registry machinery stays with it.

## Presenters: how a frame crosses to the UI thread in canvas mode

In canvas mode the canvas context renders into textures owned by a
`FramePresenter` (`cpp/rnwgpu/FramePresenter.h`), the producer-side
interface `SurfaceInfo` drives from the rendering thread: `configure`,
`unconfigure`, `releaseForDevice`, `resize` (the drawing buffer size, kept
equal to `canvas.width`/`canvas.height` like the swapchain), `getCurrentTexture`
and `present`. The view consumes finished frames from the concrete presenter
on the UI thread. Two exist:

**`BlitPresenter`** (`cpp/rnwgpu/BlitPresenter.h`, portable C++, used on
Android and iOS). Two textures: the rendering thread draws into the back one,
`present()` swaps it to the front and wakes the view. On the UI thread,
`presentFrame()` builds and configures a `wgpu::Surface` for the view's
window or layer the first time it is needed, acquires its texture, copies the
front frame into it on the device queue and presents it. Queue order makes
the copy read a finished frame, so no fence is needed. A frame that was not
copied before the next one finished is skipped. The surface is configured
from the frame (format and size) with `RenderAttachment | CopyDst` only, so
it never sees the canvas's `viewFormats` or extra usages. It works with any
device and any texture configuration the surface can be a copy destination
for. `presentFrame()` returns `Idle` (nothing new), `Presented` (a buffer was
queued; do not call again until the platform consumed it) or `Retry` (a frame
is waiting but the surface could not take it right now, e.g. mid-resize).

**`HardwareBufferPresenter`** (`cpp/rnwgpu/HardwareBufferPresenter.h`,
Android 10+, opt-in through `android={{ surfaceType: "HardwareBufferView" }}`).
A pool of `AHardwareBuffer`s imported as `SharedTextureMemory`; a waiter
thread blocks on each frame's render-complete fence and the view then draws
the buffer inline with `Bitmap.wrapHardwareBuffer`, with no `SurfaceTexture`
and no copy. Documented in full in `android/WebGPUHardwareBufferView.md`.
When the pool cannot serve the canvas (device without AHardwareBuffer
sharing, a format other than `rgba8unorm`, `viewFormats`, an unsupported
usage, a failed allocation, also after a reconfigure to such a setup), it
reports it once and `WebGPUView` replaces the view with a
`WebGPUBlitTextureView`, sticky until `surfaceType` changes. Measured on the
Pixel 8 it is not a latency win over the blit presenter; what it buys is the
rest of the UI staying smooth when the canvas is GPU-bound. That is why it is
opt-in and not part of `auto`.

An IOSurface presenter for Apple platforms (the zero-copy counterpart, via
`layer.contents`) is not built; iOS uses the blit presenter.

`SurfaceInfo` (`cpp/rnwgpu/SurfaceRegistry.h`) owns both presenters, turns
them on and off from the view glue (`enableBlit`, `enablePool`, `detach` on
the way out) and routes `getCurrentTexture`/`presentFrame` to whichever is
enabled. When no presenter is enabled the context is in swapchain mode (a
surface attached) or offscreen (no surface: frames go to an offscreen texture
that is handed to the next surface or presenter, so a canvas that rendered
once shows up without waiting for a render).

## Pacing: when the UI thread presents

The rendering thread is free-running (`requestAnimationFrame` on whichever
runtime renders). The UI thread presents at most once per display refresh,
and only when the platform can take the frame:

**Android** (`FrameScheduler.java`, shared by the two blit views through
`BlitPresenterClient.java`). The rendering thread's frame-ready wake-up is
coalesced onto the main looper. A `SurfaceView` frame is then presented in a
Choreographer frame callback: the compositor takes its buffer at the vsync. A
`TextureView` frame is instead posted to the main looper, which runs it
behind any draw the window has pending, and the next one waits for
`onSurfaceTextureUpdated`, the draw pass in which HWUI latched the previous
buffer. The reason is the TextureView buffer queue: it holds one frame and
silently replaces a frame the window has not latched yet, so presents that
run ahead of the window's draws show every other frame. This is the pacing
react-native-skia arrived at in wcandillon/react-native-skia#4129, where the
same half rate was measured and fixed. A frame the surface could not take is
retried on the next vsync, never straight away. The scheduler is plain Java
and `FrameSchedulerTest` (JVM, run with
`:react-native-webgpu:testDebugUnitTest`) asserts the exact call sequence
for both kinds.

**iOS** (`MetalView.mm`). A `CADisplayLink` at the display's maximum rate
calls `presentFrame()` once per tick; acquiring a drawable faster than the
display consumes them would block the main thread. The link only ticks while
frames keep coming: after about a quarter of a second without one it pauses,
and the next frame-ready wake-up restarts it.

What is not paced is the rendering thread itself: under GPU load it renders
frames that are never shown (see the measurements). Pacing it belongs with a
canvas-owned frame loop, not with the presenters.

## Threading and lifecycle

Canvas mode has a simple rule: the UI thread owns the native surface and the
presenter's consumer side; the rendering thread owns the presenter's
producer side; the presenter guards its state with its own mutex, and
`SurfaceInfo` may call into it while holding its own (lock order
`SurfaceInfo` then presenter), except for `getCurrentTexture()`, which may
block. The one Dawn object both threads use is the device, whose own mutex
serializes the copy on the UI thread with the rendering thread's submits.

- **Surface available.** The view enables its presenter with a closure that
  creates the `wgpu::Surface` from its window or layer, and asks for a
  present right away, so the latest frame (rendered offscreen before the view
  existed, or on a previous surface) shows without waiting for a render.
- **Surface destroyed.** The view disables the presenter synchronously,
  before it releases the window: the presenter drops its surface and hands
  its latest frame back, which becomes the context's offscreen texture. The
  context keeps rendering; `present()` is a no-op until a surface is back.
  Backgrounding and view replacement (changing `mode`, `surfaceType`,
  `zOrderOnTop`, or the hardware buffer fallback) are this path followed by
  "surface available" on the new view, which is how the frame carries across.
- **Device going away.** `GPUDevice::destroy()` calls `releaseForDevice`
  through `SurfaceInfo::releaseSurfaceForDevice` before destroying the
  device: the presenters drop every Dawn object that belongs to it, on the
  calling thread, including the blit presenter's own swapchain. Left to the
  view, those would be released from the UI thread after the device is gone
  and abort inside Dawn.
- **Reconfigure.** `configure()` with a new device drops the frames and the
  blit surface (on Vulkan a surface cannot move to another device). A new
  format bumps the texture generation; the front frame is still shown, just
  never rendered into again.
- **Drop.** `WebGPUViewManager.onDropViewInstance` (Android) and `MetalView
  dealloc` (Apple) retire the registry entry; the JS context keeps its own
  reference until it is released.

Swapchain mode keeps the previous design: the surface is attached latched (the
UI thread stores it as pending, the rendering thread adopts it at the next
frame boundary), detached immediately, and frames are tracked with an epoch
so a present never lands on a surface acquired for another frame. The header
comment of `SurfaceRegistry.h` describes it.

## Files

| File | Role |
| --- | --- |
| `src/Canvas.tsx` | The `mode` prop and the Android `surfaceType` / `zOrderOnTop` props |
| `cpp/rnwgpu/FramePresenter.h` | Producer-side interface of a canvas-mode presenter |
| `cpp/rnwgpu/BlitPresenter.h` | The copy presenter (Android + iOS) |
| `cpp/rnwgpu/HardwareBufferPresenter.h` | The AHardwareBuffer pool presenter (Android, opt-in) |
| `cpp/rnwgpu/SurfaceRegistry.h` | `SurfaceInfo`: presenter switching, offscreen fallback, and the swapchain-mode latching |
| `android/.../WebGPUView.java` | Resolves mode + surface type + opacity to one of five child views |
| `android/.../FrameScheduler.java` | When a blit view presents (vsync vs behind the pending draw) |
| `android/.../BlitPresenterClient.java` | The Java side of the blit presenter: wake-up, scheduler host, JNI |
| `android/.../WebGPUBlitSurfaceView.java`, `WebGPUBlitTextureView.java` | Canvas-mode views (copy) |
| `android/.../WebGPUHardwareBufferView.java` | Canvas-mode view (hardware buffers) |
| `android/.../WebGPUSurfaceView.java`, `WebGPUTextureView.java` | Swapchain-mode views |
| `android/cpp/cpp-adapter.cpp` | JNI for all of the above |
| `android/src/test/.../FrameSchedulerTest.java` | Scheduler unit tests |
| `apple/MetalView.mm` | Both modes on Apple platforms (`canvasMode` flag, display link) |
| `apps/example/src/Diagnostics/FramePacing.tsx` | Measurement screen (rendered rate, setup toggles) |
| `apps/example/src/Diagnostics/TransparencyMode.tsx` | Every view and mode on one canvas |

## Measurements

Pixel 8 (Android 16, 120 Hz display, debug build, Metro attached), 2026-10-06,
with the `FramePacing` diagnostic: a full-screen fragment shader on every
`requestAnimationFrame`, whose loop count ("load") sets the GPU cost of a
frame. "Shown" is the compositor's frame count for the canvas's layer over a
6 s window (`dumpsys SurfaceFlinger --timestats`: the SurfaceView's BLAST
buffer layer, or the app window's VRI layer for a TextureView or
HardwareBufferView), "rendered" the `requestAnimationFrame` callbacks the JS
loop completed per second, "janky" the share of HWUI window frames over
budget (meaningless for a SurfaceView, whose window does not redraw). One
run per cell, so treat small differences as noise.

| Setup | Native view | Load 0 | Load 25 | Load 80 | Load 120 |
| --- | --- | --- | --- | --- | --- |
| canvas, opaque (default) | `WebGPUBlitSurfaceView` | 104 / 101 | 106 / 103 | 104 / 102 | 79 / 78 |
| canvas, non-opaque (default) | `WebGPUBlitTextureView` | 118 / 116, 0.0% | 117 / 115, 0.6% | 20 / 24, 1.2% (see below) | 73 / 75, 19% |
| swapchain, non-opaque | `WebGPUTextureView` | 113 / 111, 0.1% | 114 / 112, 0.3% | 105 / 103, 2.1% | 78 / 77, 5.3% |
| swapchain, opaque | `WebGPUSurfaceView` | 106 / 102 | 103 / 101 | 106 / 104 | 74 / 73 |
| canvas, `HardwareBufferView` | `WebGPUHardwareBufferView` | 117 / 114, 0.3% | 114 / 112, 1.0% | 59 / 58, 4.0% | 60 / 59, 1.3% |

Cells are shown fps / rendered fps, then HWUI janky where it applies.

What the numbers say:

- **Presentation loses nothing.** In every setup the shown rate equals the
  rendered rate: the canvas-mode TextureView shows every frame the loop
  renders, which is the point of presenting behind the pending draw (a
  TextureView presented ahead of its window's draws shows every other one).
  The rendering thread, not presentation, is the ceiling: the JS loop of this
  screen tops out around 115 fps with a TextureView and around 104 fps with
  a SurfaceView on this debug build, in both modes.
- **Canvas mode costs nothing measurable at low and moderate load** against
  swapchain mode with the same view (118 against 113 at load 0, 117 against
  114 at load 25 for the TextureView; 104 against 106 for the SurfaceView).
  The one extra copy per frame does not show at this resolution.
- **When the GPU is saturated, the unpaced rendering thread shows.** At
  load 120 (about 13 ms of GPU per frame) all setups are GPU-bound around 75
  fps, and the canvas-mode TextureView paid with 19% of janky window frames
  against 5% in swapchain mode in this run: the JS loop keeps submitting
  frames the GPU cannot keep up with, each copy queues behind them, and HWUI
  waits on the copied buffer's fence when it draws. In swapchain mode the
  FIFO swapchain blocks `getCurrentTexture()` instead, which paces the loop
  at the display rate. One run, and the load 80 and 120 cells are also
  subject to the next point, so take the jank column as an indication.
- **The load 80 and 120 cells are shaped by Android's adaptive refresh
  rate, in both modes.** Repeating the load 80 cell with the display mode
  recorded (`dumpsys display`, `renderFrameRate`): the swapchain-mode
  TextureView ran at 113 fps at 120 Hz in one repetition, and at 30 fps and
  30 fps in two others, where the display had switched to 30 Hz and then 60
  Hz; the hardware buffer view, once it had rendered at about 60 fps under
  load, stayed at 59 fps even at load 0 until it was remounted. The 20 fps
  cell above is the same thing in canvas mode. SurfaceFlinger infers a
  content rate from the cadence of the layer's frames, switches the display
  (120, 60, 40, 30, 24 and 20 Hz on the Pixel 8) or the app's vsync to match,
  and since `requestAnimationFrame` follows the app's vsync, the loop then
  renders at that rate, which confirms the inference: a lock-in that
  irregular frame timing under GPU load triggers in any setup. The remedy is
  to vote a frame rate for the canvas's surface (`Surface.setFrameRate`, API
  30, or `View.setRequestedFrameRate`, API 35), which react-native-skia is
  adding on its SurfaceView path (wcandillon/react-native-skia#4102). Not
  done here; it is a policy decision (a vote for the display's peak rate
  costs power on static canvases) that belongs with the frame loop work.
  Until then, measure at loads where the loop keeps up, or pin the display
  mode (`adb shell settings put system min_refresh_rate 120`) when
  measuring.

## Status and open items

- Android canvas mode is verified on the Pixel 8 (Android 16, 120 Hz): every
  view and mode switch on a live canvas, hide/show, background/foreground,
  the hardware buffer fallback, and the rates above.
- iOS canvas mode is verified on the iOS 26.5 simulator only (static and
  animated frames, resize, rgba16float, translucency, device destroy, mode
  switching, Cube, three.js, Reanimated on the UI thread and on a dedicated
  thread). It has not been measured on a physical iPhone: the copy cost at
  120 Hz and the main-thread device contention are the two things to check
  before shipping the default. Reverting the iOS default is one line in
  `WebGPUView.mm`.
- macOS has no canvas mode; `MetalView` ignores the flag there.
- The rendering thread is not paced: under GPU load it renders frames that
  are never shown. A canvas-owned frame loop (the view driving
  `requestAnimationFrame` from its vsync) is the fix, and would also let
  `present()` become optional.
- The swapchain mode API is still the acquire/present pair the web API
  implies (`getCurrentTexture()` / `present()`). A scoped frame callback
  (the library hands the swapchain texture to a callback and presents when it
  returns) would bound the wait on teardown and replace the latching
  machinery with a mutex held for the frame; it has not been designed in
  detail.
- `viewFormats` on the swapchain path crash on Android with the pinned Dawn
  (`android/ViewFormatsSwapchainCrash.md`); canvas mode is immune, since the
  blit presenter never configures the surface with them.
- An IOSurface presenter (zero copy on Apple) is not built; build it only
  with a device attached, after measuring the copy.
