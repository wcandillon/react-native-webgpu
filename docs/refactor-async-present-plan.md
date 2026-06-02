# Refactor: event-driven async + auto-present

Status: **planning / Phase 0 (local spikes)**
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

## Implementation phases (after Phase 0)

**Phase 1 — Event-driven async** (no public API change; `present()` untouched)
- Add `RuntimeScheduler` (+ main-runtime CallInvoker impl) and `GpuEventLoop`.
- Switch all 7 async sites to `WaitAnyOnly` + `GpuEventLoop.addFuture(...)`:
  `api/GPU.cpp`, `api/GPUAdapter.cpp`, `api/GPUDevice.cpp` (×3), `api/GPUBuffer.cpp`,
  `api/GPUQueue.cpp`, `api/GPUShaderModule.cpp`.
- Delete `async/AsyncRunner.*` polling + `async/JSIMicrotaskDispatcher.*`; keep
  `AsyncTaskHandle` / `Promise` settle path on the new scheduler.

**Phase 2 — Auto-present + remove `present()`**
- Add `FrameDriver` (iOS `CADisplayLink`, Android `AChoreographer`); wire
  `getCurrentTexture` → register; vsync → dispatch present to owning runtime.
- Remove `GPUCanvasContext::present` (`api/GPUCanvasContext.h:50,58`, `.cpp:56-65`) and
  `SurfaceInfo::present` (`SurfaceRegistry.h:116-121`).
- JS: drop `present` from `RNCanvasContext` (`src/Canvas.tsx:22-24`, `src/types.ts`).
- Migrate all 16 example / `useWebGPU` call sites + `README.md` + `packages/webgpu/README.md`.

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
