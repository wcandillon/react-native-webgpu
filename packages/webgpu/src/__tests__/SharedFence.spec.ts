import { client } from "./setup";

// Exercises the GPUSharedFence loop: endAccess produces { fence, signaledValue }
// pairs, export() yields a native handle, importSharedFence re-imports it, and
// it feeds back into beginAccess as a wait fence. The cross-queue wait semantics
// aren't unit-testable here (they need a second consumer on another queue), so
// this checks the API shape and that handles round-trip through native.

// What the on-device eval reports back. BigInt isn't JSON-serializable across
// the eval boundary, so handle and signaledValue come back as decimal strings.
// The round-trip fields are null unless endAccess produced at least one fence.
interface FenceProbe {
  fenceCount: number;
  brand: string | null;
  type: string | null;
  handle: string | null;
  signaledValue: string | null;
  reimportedBrand: string | null;
  beganWithWaitFence: boolean;
}

type EvalResult =
  | { kind: "skip"; reason: string }
  | { kind: "fail"; reason: string }
  | ({ kind: "ok" } & FenceProbe);

describe("SharedFence", () => {
  it("endAccess surfaces { fence, signaledValue } pairs that export, re-import, and feed beginAccess", async () => {
    const result = await client.eval<Record<string, never>, EvalResult>(
      ({ device }) => {
        const FEATURE = "rnwebgpu/shared-texture-memory";
        if (!device.features.has(FEATURE as GPUFeatureName)) {
          return {
            kind: "skip",
            reason: `${FEATURE} not enabled on this device`,
          };
        }
        if (typeof RNWebGPU?.createTestVideoFrame !== "function") {
          return {
            kind: "skip",
            reason: "RNWebGPU.createTestVideoFrame is unavailable",
          };
        }

        let frame: ReturnType<typeof RNWebGPU.createTestVideoFrame> | null =
          null;
        try {
          frame = RNWebGPU.createTestVideoFrame(256, 256);
          const memory = device.importSharedTextureMemory({
            handle: frame.handle,
            label: "fence-test",
          });
          const texture = memory.createTexture();
          memory.beginAccess(texture, true);

          // endAccess only emits fences if the queue actually used the shared
          // texture during the access; a bare begin/end submits no work and
          // returns none. Sample the texture in a real pass so a fence is
          // produced.
          const module = device.createShaderModule({
            code: /* wgsl */ `
              @vertex fn vs(@builtin(vertex_index) vid: u32) -> @builtin(position) vec4f {
                var p = array<vec2f, 3>(
                  vec2f(-1.0, -3.0), vec2f(-1.0, 1.0), vec2f(3.0, 1.0),
                );
                return vec4f(p[vid], 0.0, 1.0);
              }
              @group(0) @binding(0) var srcTex: texture_2d<f32>;
              @group(0) @binding(1) var srcSampler: sampler;
              @fragment fn fs(@builtin(position) pos: vec4f) -> @location(0) vec4f {
                return textureSample(srcTex, srcSampler, pos.xy / 256.0);
              }
            `,
          });
          const pipeline = device.createRenderPipeline({
            layout: "auto",
            vertex: { module, entryPoint: "vs" },
            fragment: {
              module,
              entryPoint: "fs",
              targets: [{ format: "rgba8unorm" }],
            },
            primitive: { topology: "triangle-list" },
          });
          const sampler = device.createSampler({
            magFilter: "linear",
            minFilter: "linear",
          });
          const bindGroup = device.createBindGroup({
            layout: pipeline.getBindGroupLayout(0),
            entries: [
              { binding: 0, resource: texture.createView() },
              { binding: 1, resource: sampler },
            ],
          });
          const target = device.createTexture({
            size: [64, 64],
            format: "rgba8unorm",
            usage: GPUTextureUsage.RENDER_ATTACHMENT,
          });
          const encoder = device.createCommandEncoder();
          const pass = encoder.beginRenderPass({
            colorAttachments: [
              {
                view: target.createView(),
                clearValue: { r: 0, g: 0, b: 0, a: 1 },
                loadOp: "clear",
                storeOp: "store",
              },
            ],
          });
          pass.setPipeline(pipeline);
          pass.setBindGroup(0, bindGroup);
          pass.draw(3);
          pass.end();
          device.queue.submit([encoder.finish()]);

          const state = memory.endAccess(texture);

          if (
            typeof state !== "object" ||
            state === null ||
            typeof state.initialized !== "boolean" ||
            !Array.isArray(state.fences)
          ) {
            return {
              kind: "fail",
              reason: "endAccess state has the wrong shape",
            };
          }

          const probe: FenceProbe = {
            fenceCount: state.fences.length,
            brand: null,
            type: null,
            handle: null,
            signaledValue: null,
            reimportedBrand: null,
            beganWithWaitFence: false,
          };

          // If Dawn produced signal fences, round-trip one through native:
          // export the handle, re-import it, and consume it as a wait fence.
          if (state.fences.length > 0) {
            // eslint-disable-next-line prefer-destructuring
            const first = state.fences[0];
            const { fence, signaledValue } = first;
            const info = fence.export();
            const reimported = device.importSharedFence({
              type: info.type,
              handle: info.handle,
            });
            try {
              memory.beginAccess(texture, true, [
                { fence: reimported, signaledValue },
              ]);
              probe.beganWithWaitFence = true;
              memory.endAccess(texture); // close the second access window
            } catch {
              probe.beganWithWaitFence = false;
            }
            probe.brand = fence.__brand;
            probe.type = info.type;
            probe.handle = info.handle.toString();
            probe.signaledValue = signaledValue.toString();
            probe.reimportedBrand = reimported.__brand;
          }

          target.destroy();
          texture.destroy();
          return { kind: "ok" as const, ...probe };
        } catch (e) {
          return { kind: "fail", reason: `${(e as Error).message ?? e}` };
        } finally {
          frame?.release();
        }
      },
    );

    if (result.kind === "skip") {
      console.log(`SharedFence: skipping (${result.reason})`);
      return;
    }
    if (result.kind === "fail") {
      throw new Error(`SharedFence: ${result.reason}`);
    }

    if (result.fenceCount > 0) {
      expect(result.brand).toBe("GPUSharedFence");
      expect([
        "mtl-shared-event",
        "sync-fd",
        "vk-semaphore-opaque-fd",
      ]).toContain(result.type);
      expect(result.handle).toMatch(/^\d+$/);
      expect(result.handle).not.toBe("0");
      expect(result.signaledValue).toMatch(/^\d+$/);
      expect(result.reimportedBrand).toBe("GPUSharedFence");
      expect(result.beganWithWaitFence).toBe(true);
    } else {
      // Don't silently pass over missing coverage.
      console.log(
        "SharedFence: endAccess produced no fences on this platform; round-trip not exercised",
      );
    }
  });
});
