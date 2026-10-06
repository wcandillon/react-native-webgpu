# Android: a canvas configured with `viewFormats` aborts inside Dawn

## Summary

On Android, configuring a canvas with `viewFormats` kills the app with `SIGABRT` on the first `getCurrentTexture()`, when the canvas is backed by a `SurfaceView` or a `TextureView` (the two defaults).

It is a Dawn bug in the Vulkan swapchain, not a bug in this package. It is fixed upstream since 2026-09-21. The Dawn this package pins (`chrome-m154`, commit `3d786993`, 2026-08-31) predates the fix, so the crash goes away with a Dawn bump and nothing needs to change in our code.

## Symptom

The process aborts on the rendering thread. There is no abort message and no Dawn log line in logcat, only the stack:

```
signal 6 (SIGABRT), code -1 (SI_QUEUE)
#00 abort
#01 dawn::native::SwapChainBase::GetCurrentTexture()
#02 dawn::native::Surface::GetCurrentTexture(dawn::native::SurfaceTexture*) const
#03 dawn::native::Surface::APIGetCurrentTexture(dawn::native::SurfaceTexture*) const
#04 wgpu::Surface::GetCurrentTexture(wgpu::SurfaceTexture*) const
#05 rnwgpu::SurfaceInfo::acquireSurfaceTextureLocked()
#06 rnwgpu::SurfaceInfo::getCurrentTexture(unsigned long*)
```

The same abort is reached through `SurfaceInfo::applyPendingAttach()` and `blitOffscreenToSurfaceLocked()` when a surface attaches to a canvas that was already configured.

## Reproduce

Seen on a Pixel 8 running Android 17.

The `viewFormats Use-After-Free` diagnostic in the example app (Tests, then that entry) crashes as soon as it opens. It was written for another bug, but it is the smallest existing repro: a default canvas configured with `viewFormats`.

In code:

```tsx
const context = ref.current!.getContext("webgpu")!;
const format = navigator.gpu.getPreferredCanvasFormat(); // rgba8unorm
context.configure({
  device,
  format,
  viewFormats: [`${format}-srgb`],
});
context.getCurrentTexture(); // SIGABRT
```

## Cause

References are to the pinned Dawn sources in `externals/dawn`.

- `src/dawn/native/vulkan/SwapChainVk.cpp:555`: the swapchain image is wrapped in a texture whose descriptor only carries the size, the format and the usage. The configuration's view formats are left out.
- `src/dawn/native/SwapChain.cpp:129`: `SwapChainBase::GetCurrentTexture()` then checks that the texture's view formats equal the configured ones with a `DAWN_CHECK`, which aborts.

The check passes when Dawn takes its internal blit path, because that texture is created from the full descriptor (`SwapChainVk.cpp:571`). Dawn takes that path when the requested size is outside what the surface supports (`:265`) or when the surface does not support the requested usage (`:276`). So `viewFormats` can happen to work on Android today, depending on the size and usage. That is from reading the source; no such configuration was run here.

Metal builds the swapchain texture from the full descriptor (`src/dawn/native/metal/SwapChainMTL.mm:122`), so iOS and macOS should not be affected. That is from reading the source; it was not run here.

## Upstream status

The fix is the change "Route surfaces configured with viewFormats through the blit path", contributed from this project: any non-empty `viewFormats` now forces the blit path. State as of 2026-10-05:

| Where | State |
| --- | --- |
| [google/dawn#73](https://github.com/google/dawn/pull/73) (GitHub) | still shown as open |
| [Gerrit 332275](https://dawn-review.googlesource.com/c/dawn/+/332275), the import the GitHub PR links to | abandoned on 2026-09-18; the same change landed as 325476 |
| [Gerrit 325476](https://dawn-review.googlesource.com/c/dawn/+/325476), the same change | merged on 2026-09-21, commit `f92edf25efce9113ef66b777e47f8e8f8cca61af` |
| [Gerrit 325477](https://dawn-review.googlesource.com/c/dawn/+/325477), "Support VK_KHR_swapchain_mutable_format" | merged on 2026-09-21, commit `6d79febd8dfa17ef83f76ee57100d8a502a6e818` |

The pinned commit does not contain either of them.

## Who is affected in this package

| Canvas backing | With the pinned Dawn |
| --- | --- |
| `SurfaceView` (default, opaque) | crashes, reproduced |
| `TextureView` (default, `opaque={false}`) | crashes, reproduced |
| `HardwareBufferView` once the copy fallback is in | works: the canvas falls back to the copy view, which never passes view formats to the swapchain. Switching `surfaceType` to `SurfaceView` or `TextureView` at runtime on such a canvas hits the crash. |
| iOS, macOS | not expected to be affected, not run |

## What to do

- **Do not strip `viewFormats` in `SurfaceInfo::configureSurfaceLocked`.** It would turn the crash into a validation error, but it would also break the configurations where Dawn takes its blit path and view formats work today.
- **Bump Dawn** to a revision that contains `f92edf25`. No change is needed in this package.
- **After the bump**, open the `viewFormats Use-After-Free` diagnostic on Android. It should render, and keep rendering after its resize button.
- **Until then**, do not pass `viewFormats` on Android with the default views.
