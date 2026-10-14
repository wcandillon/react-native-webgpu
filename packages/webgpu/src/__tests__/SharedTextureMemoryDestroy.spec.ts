import { client } from "./setup";

// destroy() on GPUSharedTextureMemory and GPUSharedFence releases the native
// objects right away. A later use of the destroyed object is reported like
// Dawn reports an invalid object: a validation error that error scopes catch.

type EvalResult<R> =
  | { kind: "skip"; reason: string }
  | { kind: "fail"; reason: string }
  | ({ kind: "ok" } & R);

interface MemoryProbe {
  destroyError: string | null;
  createTextureError: string | null;
  errorTextureViewError: string | null;
  describedTextureError: string | null;
  beginAccessError: string | null;
  endAccessError: string | null;
  beginAccessThrew: boolean;
  endAccessThrew: boolean;
  errorTexture: { brand: string; width: number; height: number };
  describedTexture: {
    label: string;
    mipLevelCount: number;
    width: number;
    height: number;
  };
}

interface FenceProbe {
  exportedType: string;
  waitThrew: string | null;
  waitError: string | null;
  exportThrew: boolean;
  exportError: string | null;
  beginAccessError: string | null;
  beginAccessThrew: boolean;
}

interface OpenAccessProbe {
  destroyError: string | null;
  endAccessThrew: boolean;
  endAccessError: string | null;
}

const logSkip = (reason: string) =>
  console.log(`SharedTextureMemoryDestroy: skipping (${reason})`);

describe("SharedTextureMemoryDestroy", () => {
  it("reports the use of a destroyed memory as a validation error", async () => {
    const result = await client.eval<
      Record<string, never>,
      EvalResult<MemoryProbe>
    >(({ device }) => {
      const NATIVE_IMPORT = [
        "shared-texture-memory-iosurface",
        "shared-texture-memory-ahardware-buffer",
      ];
      if (
        !NATIVE_IMPORT.some((f) => device.features.has(f as GPUFeatureName))
      ) {
        return {
          kind: "skip",
          reason: "native frame import is unavailable on this device",
        };
      }
      if (typeof RNWebGPU?.createTestVideoFrame !== "function") {
        return {
          kind: "skip",
          reason: "RNWebGPU.createTestVideoFrame is unavailable",
        };
      }

      const frame = RNWebGPU.createTestVideoFrame(64, 32);
      try {
        const memory = device.importSharedTextureMemory({
          handle: frame.handle,
          label: "destroyed",
        });
        const texture = memory.createTexture();
        const { format } = texture;
        const { usage } = texture;
        memory.beginAccess(texture, true);
        memory.endAccess(texture);
        texture.destroy();

        const errors: Promise<GPUError | null>[] = [];
        // Destroying after the access ended is fine, and a second call does
        // nothing.
        device.pushErrorScope("validation");
        memory.destroy();
        memory.destroy();
        errors.push(device.popErrorScope());

        device.pushErrorScope("validation");
        const errorTexture = memory.createTexture();
        errors.push(device.popErrorScope());

        // An error texture: using it is a validation error too.
        device.pushErrorScope("validation");
        errorTexture.createView();
        errors.push(device.popErrorScope());

        // With a descriptor, the error texture is described by it, as Dawn's.
        device.pushErrorScope("validation");
        const describedTexture = memory.createTexture({
          size: [64, 32],
          format,
          usage,
          mipLevelCount: 2,
          label: "after-destroy",
        });
        errors.push(device.popErrorScope());

        device.pushErrorScope("validation");
        let beginAccessThrew = false;
        try {
          memory.beginAccess(errorTexture, true);
        } catch {
          beginAccessThrew = true;
        }
        errors.push(device.popErrorScope());

        device.pushErrorScope("validation");
        let endAccessThrew = false;
        try {
          memory.endAccess(errorTexture);
        } catch {
          endAccessThrew = true;
        }
        errors.push(device.popErrorScope());

        // No array destructuring in the function evaluated on the device:
        // Babel's helper for it doesn't exist there.
        return Promise.all(errors).then((popped) => {
          frame.release();
          return {
            kind: "ok" as const,
            destroyError: popped[0]?.message ?? null,
            createTextureError: popped[1]?.message ?? null,
            errorTextureViewError: popped[2]?.message ?? null,
            describedTextureError: popped[3]?.message ?? null,
            beginAccessError: popped[4]?.message ?? null,
            endAccessError: popped[5]?.message ?? null,
            beginAccessThrew,
            endAccessThrew,
            errorTexture: {
              brand: errorTexture.__brand,
              width: errorTexture.width,
              height: errorTexture.height,
            },
            describedTexture: {
              label: describedTexture.label,
              mipLevelCount: describedTexture.mipLevelCount,
              width: describedTexture.width,
              height: describedTexture.height,
            },
          };
        });
      } catch (e) {
        frame.release();
        return { kind: "fail", reason: `${(e as Error).message ?? e}` };
      }
    });

    if (result.kind === "skip") {
      logSkip(result.reason);
      return;
    }
    if (result.kind === "fail") {
      throw new Error(`SharedTextureMemoryDestroy: ${result.reason}`);
    }
    expect(result.destroyError).toBeNull();
    expect(result.createTextureError).toContain(
      'Destroyed GPUSharedTextureMemory "destroyed" used in createTexture().',
    );
    expect(result.errorTextureViewError).toContain("is invalid");
    expect(result.describedTextureError).toContain(
      'Destroyed GPUSharedTextureMemory "destroyed" used in createTexture().',
    );
    expect(result.beginAccessError).toContain(
      'Destroyed GPUSharedTextureMemory "destroyed" used in beginAccess().',
    );
    expect(result.endAccessError).toContain(
      'Destroyed GPUSharedTextureMemory "destroyed" used in endAccess().',
    );
    // Like when Dawn rejects the access.
    expect(result.beginAccessThrew).toBe(true);
    expect(result.endAccessThrew).toBe(true);
    // An error texture, described by the memory's properties as Dawn does.
    expect(result.errorTexture).toEqual({
      brand: "GPUTexture",
      width: 64,
      height: 32,
    });
    // Or by the descriptor when there is one.
    expect(result.describedTexture).toEqual({
      label: "after-destroy",
      mipLevelCount: 2,
      width: 64,
      height: 32,
    });
  });

  it("reports the use of a destroyed fence as a validation error, but not a wait Dawn took", async () => {
    const result = await client.eval<
      Record<string, never>,
      EvalResult<FenceProbe>
    >(({ device }) => {
      const NATIVE_IMPORT = [
        "shared-texture-memory-iosurface",
        "shared-texture-memory-ahardware-buffer",
      ];
      if (
        !NATIVE_IMPORT.some((f) => device.features.has(f as GPUFeatureName))
      ) {
        return {
          kind: "skip",
          reason: "native frame import is unavailable on this device",
        };
      }
      // importSharedFence needs the matching shared-fence feature.
      const FENCE_IMPORT = [
        "shared-fence-mtl-shared-event",
        "shared-fence-sync-fd",
      ];
      if (!FENCE_IMPORT.some((f) => device.features.has(f as GPUFeatureName))) {
        return {
          kind: "skip",
          reason: "shared fence import is unavailable on this device",
        };
      }
      if (typeof RNWebGPU?.createTestVideoFrame !== "function") {
        return {
          kind: "skip",
          reason: "RNWebGPU.createTestVideoFrame is unavailable",
        };
      }

      const frame = RNWebGPU.createTestVideoFrame(64, 64);
      try {
        const memory = device.importSharedTextureMemory({
          handle: frame.handle,
          label: "fence-test",
        });
        const texture = memory.createTexture();

        // endAccess only produces a fence once the queue used the texture.
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
              return textureSample(srcTex, srcSampler, pos.xy / 64.0);
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
        const bindGroup = device.createBindGroup({
          layout: pipeline.getBindGroupLayout(0),
          entries: [
            { binding: 0, resource: texture.createView() },
            { binding: 1, resource: device.createSampler() },
          ],
        });
        const target = device.createTexture({
          size: [64, 64],
          format: "rgba8unorm",
          usage: GPUTextureUsage.RENDER_ATTACHMENT,
        });
        const sample = () => {
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
        };
        const release = () => {
          target.destroy();
          texture.destroy();
          memory.destroy();
          frame.release();
        };

        memory.beginAccess(texture, true);
        sample();
        const { fences } = memory.endAccess(texture);
        if (fences.length === 0) {
          release();
          return {
            kind: "skip",
            reason: "endAccess produced no fence on this device",
          };
        }
        // No array destructuring, see above (object destructuring compiles
        // to plain property reads).
        // eslint-disable-next-line prefer-destructuring
        const { fence, signaledValue } = fences[0];
        fence.label = "end-access";
        const exported = fence.export();

        // The wait fence is destroyed before the access waits on it: Dawn
        // keeps its own reference from beginAccess on.
        device.pushErrorScope("validation");
        let waitThrew: string | null = null;
        try {
          const wait = device.importSharedFence({
            type: exported.type,
            handle: exported.handle,
          });
          memory.beginAccess(texture, true, [{ fence: wait, signaledValue }]);
          wait.destroy();
          sample();
          memory.endAccess(texture);
        } catch (e) {
          waitThrew = `${(e as Error).message ?? e}`;
        }
        const waitError = device.popErrorScope();

        fence.destroy();
        // A second call does nothing.
        fence.destroy();
        device.pushErrorScope("validation");
        let exportThrew = false;
        try {
          fence.export();
        } catch {
          exportThrew = true;
        }
        const exportError = device.popErrorScope();

        device.pushErrorScope("validation");
        let beginAccessThrew = false;
        try {
          memory.beginAccess(texture, true, [{ fence, signaledValue }]);
        } catch {
          beginAccessThrew = true;
        }
        const beginAccessError = device.popErrorScope();

        return Promise.all([waitError, exportError, beginAccessError]).then(
          (popped) => {
            release();
            return {
              kind: "ok" as const,
              exportedType: exported.type,
              waitThrew,
              waitError: popped[0]?.message ?? null,
              exportThrew,
              exportError: popped[1]?.message ?? null,
              beginAccessError: popped[2]?.message ?? null,
              beginAccessThrew,
            };
          },
        );
      } catch (e) {
        frame.release();
        return { kind: "fail", reason: `${(e as Error).message ?? e}` };
      }
    });

    if (result.kind === "skip") {
      logSkip(result.reason);
      return;
    }
    if (result.kind === "fail") {
      throw new Error(`SharedTextureMemoryDestroy: ${result.reason}`);
    }
    expect(["mtl-shared-event", "sync-fd"]).toContain(result.exportedType);
    expect(result.waitThrew).toBeNull();
    expect(result.waitError).toBeNull();
    // Like a fence Dawn can't export: the validation error, then the throw.
    expect(result.exportError).toContain(
      'Destroyed GPUSharedFence "end-access" used in export().',
    );
    expect(result.exportThrew).toBe(true);
    expect(result.beginAccessError).toContain(
      'Destroyed GPUSharedFence "end-access" used in beginAccess().',
    );
    expect(result.beginAccessThrew).toBe(true);
  });

  it("reports a destroy() during an access as a validation error", async () => {
    const result = await client.eval<
      Record<string, never>,
      EvalResult<OpenAccessProbe>
    >(({ device }) => {
      const NATIVE_IMPORT = [
        "shared-texture-memory-iosurface",
        "shared-texture-memory-ahardware-buffer",
      ];
      if (
        !NATIVE_IMPORT.some((f) => device.features.has(f as GPUFeatureName))
      ) {
        return {
          kind: "skip",
          reason: "native frame import is unavailable on this device",
        };
      }
      if (typeof RNWebGPU?.createTestVideoFrame !== "function") {
        return {
          kind: "skip",
          reason: "RNWebGPU.createTestVideoFrame is unavailable",
        };
      }

      const frame = RNWebGPU.createTestVideoFrame(64, 32);
      try {
        const memory = device.importSharedTextureMemory({
          handle: frame.handle,
          label: "open",
        });
        const texture = memory.createTexture();
        memory.beginAccess(texture, true);

        device.pushErrorScope("validation");
        memory.destroy();
        const destroyError = device.popErrorScope();

        // The access stays open (Dawn doesn't end it either), but the memory
        // is gone: ending it is now a use of a destroyed memory.
        device.pushErrorScope("validation");
        let endAccessThrew = false;
        try {
          memory.endAccess(texture);
        } catch {
          endAccessThrew = true;
        }
        const endAccessError = device.popErrorScope();

        return Promise.all([destroyError, endAccessError]).then((popped) => {
          texture.destroy();
          frame.release();
          return {
            kind: "ok" as const,
            destroyError: popped[0]?.message ?? null,
            endAccessThrew,
            endAccessError: popped[1]?.message ?? null,
          };
        });
      } catch (e) {
        frame.release();
        return { kind: "fail", reason: `${(e as Error).message ?? e}` };
      }
    });

    if (result.kind === "skip") {
      logSkip(result.reason);
      return;
    }
    if (result.kind === "fail") {
      throw new Error(`SharedTextureMemoryDestroy: ${result.reason}`);
    }
    expect(result.destroyError).toContain(
      'Destroyed GPUSharedTextureMemory "open" with an access still open: call endAccess() first.',
    );
    expect(result.endAccessThrew).toBe(true);
    expect(result.endAccessError).toContain(
      'Destroyed GPUSharedTextureMemory "open" used in endAccess().',
    );
  });
});
