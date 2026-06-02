# Refactor: event-driven async + auto-present

Status: **Phase 0 complete — all spikes GREEN, ready for Phase 1**
Branch: `claude/keen-darwin-xeywa`

This document is the handoff for moving the async + present refactor forward. Phase 0
(spikes) needs a real local machine: installed `node_modules`, a Dawn build, and the
iOS/Android toolchains. Everything below the "How to resume locally" section is meant to
be executed on your computer, not in the web container.

---

## Goals (locked)

- **Async**: replace the JS-thread polling loop with a **background `WaitAny` GPU thread**
  (Dawn `TimedWaitAny` is already enabled — `packages/webgpu/cpp/rnwgpu/api/GPU.cpp:17-23`).
- **Present**: **remove `context.present()` entirely** (breaking) in favor of a **global
  Choreographer / CADisplayLink-driven auto-present**.
- **Scope**: first-class for **all runtimes** — main JS, the reanimated UI runtime, and
  `createWorkletRuntime` dedicated runtimes.

---

## What exists today (the two problems)

### Async (polling) — `packages/webgpu/cpp/rnwgpu/async/`
- Every async op (`requestAdapter`, `requestDevice`, `mapAsync`, `onSubmittedWorkDone`,
  `createRender/ComputePipelineAsync`, `popErrorScope`) registers a Dawn callback with
  `CallbackMode::AllowProcessEvents` and calls `AsyncRunner::postTask`.
- `AsyncRunner::requestTick` (`async/AsyncRunner.cpp:89-177`) schedules `tick()` via
  `setImmediate` / `setTimeout(4ms)` / `queueMicrotask`; `tick()` calls
  `_instance.ProcessEvents()` and **re-schedules itself while any task is "pumping"**
  (`AsyncRunner.cpp:189-191`). This is a busy reschedule loop: wasted CPU when idle, added
  latency, and `JSIMicrotaskDispatcher`'s `queueMicrotask` dispatch is only thread-safe when
  called on the runtime's own thread.

### Present (manual, non-standard)
`api/GPUCanvasContext.cpp:56-65` → `SurfaceRegistry.h:116-121` → `wgpu::Surface::Present()`.
The user must call `context.present()` after every `queue.submit` (**16 JS/TS call sites**).
No CADisplayLink/Choreographer exists; RN's `requestAnimationFrame` is the only frame driver.
On Apple, present also does a blocking `WaitForCommandsToBeScheduled` on the JS thread.

---

## Target architecture

Three new pieces:

### A. `RuntimeScheduler` — thread-safe "post to this runtime's JS thread"
Replaces `AsyncDispatcher` / `JSIMicrotaskDispatcher` (which use non-thread-safe
`queueMicrotask`).
- Interface: `void scheduleOnJS(std::function<void(jsi::Runtime&)>)`, callable from any thread.
- **Main runtime**: wraps `react::CallInvoker::invokeAsync` (already available —
  `apple/WebGPUModule.mm:70`, `android/cpp/cpp-adapter.cpp:25-29`).
- **Worklet runtimes**: wraps the worklet runtime's own thread executor from
  `react-native-worklets` 0.8.3 (**see Phase 0 spike #1**).
- Stored per-runtime in a `RuntimeContext` (the "per-JS-thread event loop"), created on first
  WebGPU use, torn down via the existing `RuntimeLifecycleMonitor` / `RuntimeAwareCache`
  (`cpp/jsi/RuntimeAwareCache.h`).

### B. `GpuEventLoop` — background `WaitAny` thread (no polling)
One per `wgpu::Instance` (effectively global).
- All async sites switch `CallbackMode::AllowProcessEvents` → **`CallbackMode::WaitAnyOnly`**,
  returning a `wgpu::Future`.
- A **small bounded thread pool**; each pending future is waited via
  `instance.WaitAny(future, /*timeout*/UINT64_MAX)` on a pool thread → genuinely event-driven,
  **zero idle CPU**, resolves the instant GPU work completes. No wake/interrupt problem (each
  thread owns one future). **See Phase 0 spike #2.**
- On completion the worker marshals the result and calls the owning runtime's
  `RuntimeScheduler.scheduleOnJS` to settle the JS Promise. `AsyncTaskHandle` / `Promise`
  settle logic is reused; `AsyncRunner` + its tick loop are deleted.
- Fallback (if concurrent `WaitAny` on one instance is unsafe): single worker thread waiting on
  the batched future set with a condition-variable re-arm.

### C. `FrameDriver` — global vsync source for auto-present
One UI-thread singleton; removes the need for `present()`.
- **iOS**: `CADisplayLink` on the main run loop. **Android**: NDK
  `AChoreographer_postFrameCallback` from C++ (API 24+, avoids JNI). **See Phase 0 spike #3.**
- Lifecycle: started when ≥1 surface is configured, stopped at 0.
- **Auto-present semantics** (spec-aligned "update the rendering" after rAF):
  1. `GPUCanvasContext::getCurrentTexture()` marks its `SurfaceInfo` dirty and registers a
     present request with `FrameDriver`, tagged with the owning runtime.
  2. Each vsync (UI thread), `FrameDriver` dispatches each dirty context's present onto its
     **owning runtime's `RuntimeScheduler`** — so `Surface.Present()` + the Apple Metal
     scheduling wait run on the same thread that did `getCurrentTexture` / `submit`, preserving
     Dawn surface thread-affinity and guaranteeing present-after-submit ordering (FIFO on that
     loop). Clear dirty after present.
- Offscreen path (`SurfaceRegistry` `switchToOffscreen`, `src/Offscreen.ts`) has no surface →
  present is a no-op; tests keep reading back the CPU texture.

---

## Phase 0 — Local spikes (DO THESE FIRST, on your machine)

These de-risk the refactor before any large change. Run from repo root.

```bash
# 0. install deps (web container can't do this)
yarn install
```

### Spike 1 — worklet-runtime scheduler (HIGHEST RISK)
Goal: obtain a **thread-safe** "schedule this lambda on runtime R's thread" for an arbitrary
worklet runtime (UI runtime + a `createWorkletRuntime` runtime) using
`react-native-worklets@0.8.3`.

```bash
# inspect the worklets native API actually shipped at 0.8.3
find node_modules/react-native-worklets -name "*.h" | grep -iE "Runtime|Scheduler|Invoker|Queue"
# look for: WorkletRuntime, RuntimeManager / WorkletsModuleProxy, UIScheduler / JSScheduler,
# and any per-runtime executor / async queue we can call from a background C++ thread.
```
Deliverable: a one-paragraph note on the exact symbol(s) to use (or "not exposed → needs JS
shim / worklets PR"). This determines whether Phase 3 (first-class worklet runtimes) is cheap
or needs a workaround.

### Spike 2 — concurrent `WaitAny` on one Dawn instance
Goal: confirm multiple threads can each call `instance.WaitAny(singleFuture, UINT64_MAX)`
concurrently on the **same** instance safely. If not, switch `GpuEventLoop` to the
single-worker + condition-variable fallback.
- Search Dawn headers/docs in `externals/dawn` (or built `libs/`) for `WaitAny` threading
  guarantees. A tiny throwaway C++ test against the built Dawn is ideal.

### Spike 3 — Android frame callback
Goal: confirm NDK `AChoreographer_postFrameCallback` is usable at the project `minSdk`
(`packages/webgpu/android/build.gradle`). If `minSdk < 24` for that API, plan the Java
`Choreographer` + JNI bridge instead.

---

## Phase 0 — Findings (completed 2026-06-02, branch `claude/keen-darwin-xeywa`)

Environment verified: `node_modules` installed, `externals/dawn` present, RN **0.81.4**,
`react-native-worklets` **0.8.3**, Android `minSdk` **26**, NDK 26/27 available.

### Spike 1 — worklet-runtime scheduler → **GREEN (symbol exists, thread-safe)**
`worklets/WorkletRuntime/WorkletRuntime.h` exposes exactly what we need:
- `WorkletRuntime::schedule(std::function<void(jsi::Runtime &)> job)` — posts `job` onto the
  runtime's own `AsyncQueue` (`WorkletRuntime.cpp:211-227`). It is **callable from any thread**
  (the underlying `AsyncQueueImpl` is a mutex+condvar queue; `AsyncQueueUI` forwards to the
  `UIScheduler`). The job runs on the runtime's event-loop thread, under `runtimeMutex_`, and
  uses `weak_from_this()` so it is a **safe no-op if the runtime was torn down**. This is a
  drop-in for `RuntimeScheduler::scheduleOnJS` for worklet runtimes.
- `WorkletRuntime::getWeakRuntimeFromJSIRuntime(jsi::Runtime &rt)` (RN ≥ 0.81, we have 0.81.4)
  maps a bare `jsi::Runtime&` → `weak_ptr<WorkletRuntime>`, so the per-runtime
  `RuntimeContext` can recover the scheduler from any worklet runtime (UI + dedicated
  `createWorkletRuntime`) with no JS shim.

**Caveat (build wiring, not API):** webgpu does **not** currently link worklets natively
(no worklets entry in `packages/webgpu/*.podspec` or `android/CMakeLists.txt`; only JS-level
serialization helpers exist). Phase 3 must add the native dependency:
- iOS: depend on `RNWorklets` pod (it ships public headers under `worklets/`,
  `header_dir = "worklets"`).
- Android: import the worklets **prefab** module `worklets` (`prefabPublishing` is on in
  `react-native-worklets/android/build.gradle`).
Worklets is already a `peerDependency`, so this adds no new install. Phase 3 stays cheap; no
worklets PR or JS shim needed.

### Spike 2 — concurrent `WaitAny` on one instance → **GREEN (designed for it)**
Dawn's native `EventManager` (`externals/dawn/src/dawn/native/EventManager.{h,cpp}`) is built
for multi-threaded waits:
- State is `MutexProtected<EventState>`; `mNextFutureID` is atomic; a code comment
  (`EventManager.h:78-82`) explicitly notes "another thread can race to complete the event …
  via a WaitAny call".
- Each `WaitAny` call with a non-zero timeout creates a **stack-local `Waiter`** with its **own**
  `MutexCondVarProtected<bool>` (`EventManager.cpp:338`, `:106`), registers it per-FutureID in
  the shared map, then blocks on its own condvar. `SetFutureReady` signals the registered
  waiters. → **N threads can each block in `WaitAny` on the same instance concurrently, each
  owning its own future.** This is exactly the plan's primary "one future per pool thread" model.

**Hard constraint discovered (`EventManager.cpp:341-354`):** within a *single* `WaitAny` call
with a non-zero timeout, you may **not** mix events from multiple queues, nor a queue event
together with a non-queue event — it returns `WaitStatus::Error` ("Mixed source waits with
timeouts are not currently supported"). Note `mapAsync`/`onSubmittedWorkDone` are *queue*
events while `requestAdapter`/`requestDevice`/`createPipelineAsync`/`popErrorScope` are
*non-queue* events.
→ **Implication:** adopt the **per-future-per-thread** design (each pool thread waits on exactly
one future) — it is single-source and always legal. The plan's stated fallback ("single worker
waiting on the batched future set") is **not viable** as written, because batching mixed sources
hits this restriction. If a bounded pool is undesirable, the correct fallback is one
worker-thread *per future* (still single-source), not one worker for a batched set.

### Spike 3 — Android frame callback → **GREEN (no JNI bridge needed)**
In `android/choreographer.h`, `AChoreographer_getInstance()` and
`AChoreographer_postFrameCallback()` are both `__INTRODUCED_IN(24)`; `minSdk` is **26**, so the
pure-NDK path works with no Java `Choreographer`/JNI bridge.
- `postFrameCallback` is `__DEPRECATED_IN(29)` in favor of `postFrameCallback64` (API 29) /
  `postVsyncCallback` (API 33). Recommendation: call `postFrameCallback64` when
  `android_get_device_api_level() >= 29`, else `postFrameCallback` (works on 26-28). Both are
  acceptable; the 64-bit variant just avoids the deprecation warning and 32-bit time wrap.
- `AChoreographer_getInstance()` must be called on a thread with a `Looper` (the main/UI
  thread) — `FrameDriver` already lives on the UI thread, so this is satisfied.

### Net go/no-go
All three risks clear. Proceed to Phase 1. Two plan amendments: (1) Phase 3 must add the
worklets native build dependency (podspec + prefab); (2) `GpuEventLoop` must use
per-future-per-thread waits (drop the batched-future fallback).

## Implementation phases (after Phase 0)

**Phase 1 — Event-driven async** (no public API change; `present()` untouched) — **DONE**
- Add `RuntimeScheduler` (+ main-runtime CallInvoker impl) and `GpuEventLoop`.
- Switch all 7 async sites to `WaitAnyOnly` + `GpuEventLoop.addFuture(...)`:
  `api/GPU.cpp`, `api/GPUAdapter.cpp`, `api/GPUDevice.cpp` (×3), `api/GPUBuffer.cpp`,
  `api/GPUQueue.cpp`, `api/GPUShaderModule.cpp`.
- Delete `async/AsyncRunner.*` polling + `async/JSIMicrotaskDispatcher.*`; keep
  `AsyncTaskHandle` / `Promise` settle path on the new scheduler.

### Phase 1 — what shipped (branch `claude/keen-darwin-xeywa`)
New files (`cpp/rnwgpu/async/`):
- `RuntimeScheduler.h` — interface `scheduleOnJS(std::function<void(jsi::Runtime&)>)`,
  callable from any thread.
- `CallInvokerScheduler.{h,cpp}` — main-runtime impl wrapping
  `react::CallInvoker::invokeAsync(CallFunc&&)` (RN 0.81 delivers the job on the JS thread
  with the runtime).
- `GpuEventLoop.{h,cpp}` — background `WaitAny` driver. Lazily-grown bounded worker pool
  (cap = `clamp(hardware_concurrency, 2, 8)`); each worker does a single-future
  `instance.WaitAny(future, UINT64_MAX)` (always a legal single-source wait, per Phase 0
  spike 2). Shared state held behind a `shared_ptr` so detached workers (and the
  `wgpu::Instance` ref they need) outlive the object safely; teardown sets `running=false`
  and notifies idle workers without joining in-flight GPU waits.

Deviations from the original plan (intentional):
1. **`AsyncRunner` was replaced by `RuntimeContext`** (`async/RuntimeContext.{h,cpp}`), the
   per-runtime coordinator the plan's Target-architecture §A already named. It bundles
   `{RuntimeScheduler, GpuEventLoop}` and exposes `postTask`; all polling internals
   (`tick`/`requestTick`/`ProcessEvents`/pump counters) are gone. `AsyncTaskHandle` depends
   only on `RuntimeScheduler`. The old `AsyncRunner` name/files no longer exist anywhere
   (the 6 `api/*` classes now hold `std::shared_ptr<async::RuntimeContext> _async`); the dead
   `GPU::getAsyncRunner()` accessor was deleted.
2. **`postTask`'s callback now returns a `wgpu::Future`** (the value returned by the Dawn
   `WaitAnyOnly` call), which `AsyncRunner` hands to `GpuEventLoop.addFuture`. A returned
   future with `id == 0` means "no event to wait on" and is ignored — used by
   `GPUDevice::getLost` (resolved synchronously or later via `notifyDeviceLost`). This
   replaced the old `keepPumping` bool argument, which is gone.

`GPU`'s constructor now takes the `CallInvoker` (threaded through from `RNWebGPUManager`,
which already held it) to build the `CallInvokerScheduler`. `AsyncDispatcher.h` and
`JSIMicrotaskDispatcher.{h,cpp}` deleted; `android/CMakeLists.txt` updated (iOS podspec
globs `cpp/**` so it needs no change).

Validation run locally: all changed + new TUs syntax-check under the Android NDK toolchain;
the full `react-native-wgpu` native lib **compiles and links** for `arm64-v8a` (ninja);
`cpplint` clean (project filters); `clang-format` (pinned 15.0.0) applied; `yarn tsc` passes
(no TS changed). On-device runtime behaviour (frame pacing, zero idle CPU) is Phase 4.

**Phase 2 — Auto-present + remove `present()`** — **DONE**
- Add `FrameDriver` (iOS `CADisplayLink`, Android `AChoreographer`); wire
  `getCurrentTexture` → register; vsync → dispatch present to owning runtime.
- Remove `GPUCanvasContext::present` (`api/GPUCanvasContext.h:50,58`, `.cpp:56-65`) and
  `SurfaceInfo::present` (`SurfaceRegistry.h:116-121`).
- JS: drop `present` from `RNCanvasContext` (`src/Canvas.tsx:22-24`, `src/types.ts`).
- Migrate all 16 example / `useWebGPU` call sites + `README.md` + `packages/webgpu/README.md`.

### Phase 2 — what shipped (branch `claude/keen-darwin-xeywa`)
New files:
- `cpp/rnwgpu/FrameDriver.{h,cpp}` — global vsync auto-present coordinator. `requestPresent`
  (from `getCurrentTexture`, JS thread) coalesces per `contextId`; `onVSync` (UI thread)
  dispatches each pending surface's present onto its owning runtime's `RuntimeScheduler`
  (`surface->presentFrame()`). Request-driven: starts the platform vsync on first request,
  stops after `kMaxIdleFrames` (3) idle frames → zero idle CPU.
- `apple/WebGPUFrameDriver.{h,mm}` — iOS/tvOS `CADisplayLink` on the main run loop (paused
  toggled by start/stop). macOS uses `NSScreen.displayLinkWithTarget:` on 14+, else an
  `NSTimer` fallback. Selector → `FrameDriver::onVSync()`.
- `android/.../com/webgpu/WebGPUFrameDriver.java` — main-thread `Choreographer` driver;
  `doFrame` → static `nativeOnVSync()` JNI → `FrameDriver::onVSync()`, reposts while running.

Wiring:
- `SurfaceInfo::present()` → `presentFrame()` (Apple `WaitForCommandsToBeScheduled` + Present,
  no-op offscreen); added `SurfaceInfo::hasSurface()`. Metal extern moved to `SurfaceRegistry.h`.
- `GPU::getContext()` re-exposes the per-runtime `RuntimeContext` (so the canvas can reach its
  scheduler). `GPUCanvasContext` stores `_contextId`, registers the present in
  `getCurrentTexture` (and now sets the canvas client size there), and dropped `present()` +
  its JS binding.
- iOS `WebGPUModule install` and Android `initializeNative` register `setPlatformVSync`. View
  teardown (`MetalView dealloc`, Android `onSurfaceDestroy`) calls `FrameDriver::cancelPresent`.
- JS: `RNCanvasContext` is now just `GPUCanvasContext` (`src/Canvas.tsx`, `src/types.ts`);
  removed the no-op `present` from `Offscreen.ts` and `WebPolyfillGPUModule.ts`. 18 example
  call sites (the plan's 16 + `VisionCamera`, `ImportExternalTexture`) and both READMEs migrated.

Decisions / deviations:
1. **Android vsync = Java `Choreographer` + JNI** (not pure NDK `AChoreographer`), chosen for
   robustness — pure NDK needs a JNI hop to a Looper thread to bootstrap anyway. Confirmed with
   the user.
2. **`present()` hard-removed** (breaking), confirmed with the user.
3. **Owning-runtime caveat (→ Phase 3):** `getCurrentTexture` currently dispatches present via
   the **main** runtime's scheduler (`_gpu->getContext()`). Correct for main-JS rendering. The
   Reanimated example renders on the **UI (worklet) runtime**, so its present is migrated (call
   removed) but auto-present won't target the correct thread until Phase 3 tags the present with
   the *calling* runtime and gives worklet runtimes their own `RuntimeScheduler`. Expect the
   Reanimated/Dedicated examples to be visually broken between Phase 2 and Phase 3.

Validation (local): `react-native-wgpu` native lib **compiles and links** for `arm64-v8a`
(ninja, CMake picked up `FrameDriver.cpp`); `cpplint` clean; `clang-format` applied; `yarn tsc`
and `yarn lint` pass for both `packages/webgpu` and `apps/example`. iOS `.mm` and the Java
driver are not compiled locally (no iOS/gradle build run here) — review-only; needs a device
build. On-device frame pacing / zero-idle-CPU verification is Phase 4.

**Phase 3 — First-class worklet runtimes**
- Worklet-runtime `RuntimeScheduler` impl (per Spike 1); verify auto-present dispatch on UI +
  dedicated runtimes; update `apps/example/src/Reanimated/Reanimated.tsx` (drop `present()`,
  keep its own rAF loop).

**Phase 4 — Validation**
```bash
yarn tsc && yarn lint
yarn workspace react-native-wgpu test         # offscreen readback + demo specs
yarn build:ios        # or: yarn workspace example ios
yarn build:android    # or: yarn workspace example android
```
Verify: no idle-CPU polling (logging), correct frame pacing, no present-ordering glitches,
Reanimated UI/Dedicated examples render.

---

## 16 `present()` call sites to migrate (Phase 2)

```
apps/example/src/StorageBufferVertices/StorageBufferVertices.tsx
apps/example/src/components/useWebGPU.ts
apps/example/src/components/Texture.tsx
apps/example/src/SharedTextureMemory/SharedTextureMemory.tsx
apps/example/src/ThreeJS/Helmet.tsx
apps/example/src/ComputeToys/engine/index.ts
apps/example/src/CanvasAPI/CanvasAPI.tsx
apps/example/src/ThreeJS/PostProcessing.tsx
apps/example/src/ThreeJS/Cube.tsx
apps/example/src/Triangle/HelloTriangle.tsx
apps/example/src/Triangle/HelloTriangleMSAA.tsx
apps/example/src/ThreeJS/InstancedMesh.tsx
apps/example/src/ThreeJS/Retargeting.tsx
apps/example/src/ThreeJS/components/FiberCanvas.tsx
apps/example/src/Reanimated/Reanimated.tsx
apps/example/src/ThreeJS/Backdrop.tsx
```
Plus `README.md` and `packages/webgpu/README.md`.

---

## Risks / open questions
- **Worklet-runtime scheduler** access in worklets 0.8.3 (Spike 1 — highest risk).
- **Concurrent `WaitAny`** semantics on one Dawn instance (Spike 2; single-worker fallback ready).
- **Present timing**: vsync-dispatched-to-owning-loop must land after submit (FIFO on that loop)
  and before the next `getCurrentTexture`.
- **Breaking change**: `present()` removed — type, examples, README updated together.
- **Apple Metal wait** moves into the frame-boundary present task, off the synchronous call path.

---

## How to resume locally

```bash
git fetch origin claude/keen-darwin-xeywa
git checkout claude/keen-darwin-xeywa
git pull origin claude/keen-darwin-xeywa
# open this file and run Phase 0 spikes, then start Claude Code:
#   claude
# suggested kickoff prompt:
#   "Read docs/refactor-async-present-plan.md. Run the Phase 0 spikes and report
#    findings before implementing. Develop on this branch."
```
