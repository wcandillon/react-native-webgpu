import type {
  GPUExtent2D,
  GPUNativeBufferExternalTextureDescriptor,
  GPUSharedFenceState,
} from "../types";

import { checkImage, client, encodeImage } from "./setup";

interface BitmapData {
  data: number[];
  width: number;
  height: number;
  format: string;
}

// A pixel as [r, g, b], 0..255.
type Rgb = number[];

type FenceProbe = {
  // Whether endAccess() returned a fence, its export info, and whether the next
  // beginAccess() accepted it as a wait fence.
  returned: boolean;
  brand: string;
  type: string;
  signaledValue: string;
  acceptedAsWait: boolean;
};

type YuvResult =
  | { kind: "skip"; reason: string }
  | { kind: "fail"; reason: string }
  | {
      kind: "ok";
      wrapperBrand: string;
      externalTextureBrand: string;
      // The sampled colors of each access, in order.
      left: Rgb[];
      right: Rgb[];
      fences: FenceProbe[];
    };

type RgbResult =
  | { kind: "skip"; reason: string }
  | { kind: "fail"; reason: string }
  | ({ kind: "ok"; wrapperBrand: string; textureBrand: string } & BitmapData);

type DestroyResult =
  | { kind: "skip"; reason: string }
  | { kind: "fail"; reason: string }
  | {
      kind: "ok";
      destroyWithOpenAccess: string | null;
      secondDestroy: string | null;
      beginAfterDestroy: string | null;
      beginAfterDestroyThrew: boolean;
      endAfterDestroy: string | null;
      endAfterDestroyThrew: boolean;
      frameHandleAfterDestroy: string;
    };

type DescriptorResult =
  | { kind: "skip"; reason: string }
  | { kind: "fail"; reason: string }
  | {
      kind: "ok";
      // The error of each invalid descriptor, null when it was accepted.
      negativeOrigin: string | null;
      hugeSize: string | null;
      originOutside: string | null;
      sizeWithoutHeight: string | null;
      emptySize: string | null;
      validCrop: string | null;
      textureLabel: string;
      externalTextureLabel: string;
      acquireFenceFdType: string;
    };

const expectColor = (actual: Rgb, expected: Rgb, tolerance: number) => {
  expected.forEach((channel, i) => {
    expect(Math.abs(actual[i] - channel)).toBeLessThanOrEqual(tolerance);
  });
};

// The native buffer wrappers: a native buffer imported once, with the access
// window of each use explicit (beginAccess / endAccess), the recipe of Dawn's
// Android layer. The specs below run the same frame through several accesses
// of one import, where importExternalTexture() / importSharedTextureMemory()
// would import the buffer each time.
describe("NativeBufferWrapper", () => {
  it("samples a YUV buffer imported once as an external texture across several accesses", async () => {
    const result = await client.eval<Record<string, never>, YuvResult>(
      ({ device, gpu, ctx, canvas }) => {
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
        if (
          device.features.has(
            "shared-texture-memory-ahardware-buffer" as GPUFeatureName,
          ) &&
          !device.features.has(
            "opaque-ycbcr-android-for-external-texture" as GPUFeatureName,
          )
        ) {
          return {
            kind: "skip",
            reason: "the device cannot sample YUV hardware buffers",
          };
        }
        let frame: ReturnType<typeof RNWebGPU.createTestVideoFrame>;
        try {
          frame = RNWebGPU.createTestVideoFrame(64, 64, "nv12");
        } catch (e) {
          const message = `${(e as Error).message ?? e}`;
          if (message.indexOf("unsupported") !== -1) {
            return { kind: "skip", reason: message };
          }
          return { kind: "fail", reason: message };
        }
        try {
          // Once per buffer: the import, the texture and the external texture.
          const wrapper = device.createNativeBufferExternalTexture(frame, {
            label: "yuv-wrapper",
          });
          const module = device.createShaderModule({
            code: /* wgsl */ `
              struct VsOut {
                @builtin(position) position: vec4f,
                @location(0) uv: vec2f,
              };

              @vertex fn vs(@builtin(vertex_index) vid: u32) -> VsOut {
                var positions = array<vec2f, 3>(
                  vec2f(-1.0, -3.0),
                  vec2f(-1.0,  1.0),
                  vec2f( 3.0,  1.0),
                );
                var uvs = array<vec2f, 3>(
                  vec2f(0.0, 2.0),
                  vec2f(0.0, 0.0),
                  vec2f(2.0, 0.0),
                );
                var out: VsOut;
                out.position = vec4f(positions[vid], 0.0, 1.0);
                out.uv = uvs[vid];
                return out;
              }

              @group(0) @binding(0) var srcTex: texture_external;
              @group(0) @binding(1) var srcSampler: sampler;

              @fragment fn fs(in: VsOut) -> @location(0) vec4f {
                return textureSampleBaseClampToEdge(srcTex, srcSampler, in.uv);
              }
            `,
          });
          const pipeline = device.createRenderPipeline({
            layout: "auto",
            vertex: { module, entryPoint: "vs" },
            fragment: {
              module,
              entryPoint: "fs",
              targets: [{ format: gpu.getPreferredCanvasFormat() }],
            },
            primitive: { topology: "triangle-list" },
          });
          const sampler = device.createSampler({
            magFilter: "linear",
            minFilter: "linear",
          });
          // The bind group outlives the accesses too: the external texture is
          // the same object for every frame of the buffer.
          const bindGroup = device.createBindGroup({
            layout: pipeline.getBindGroupLayout(0),
            entries: [
              { binding: 0, resource: wrapper.externalTexture },
              { binding: 1, resource: sampler },
            ],
          });

          const readPixels = (image: BitmapData) => {
            const bgra = image.format === "bgra8unorm";
            const pixel = (x: number, y: number): Rgb => {
              const i = (y * image.width + x) * 4;
              return bgra
                ? [image.data[i + 2], image.data[i + 1], image.data[i]]
                : [image.data[i], image.data[i + 1], image.data[i + 2]];
            };
            const midY = Math.floor(image.height / 2);
            return {
              left: pixel(Math.floor(image.width / 4), midY),
              right: pixel(Math.floor((image.width * 3) / 4), midY),
            };
          };

          const left: Rgb[] = [];
          const right: Rgb[] = [];
          const fences: FenceProbe[] = [];
          // The fence of the previous access, fed to the next one as a wait
          // fence, the way a producer's fence would be.
          let waitFences: GPUSharedFenceState[] | undefined;

          const accessAndDraw = (index: number): Promise<void> => {
            const probe: FenceProbe = {
              returned: false,
              brand: "",
              type: "",
              signaledValue: "",
              acceptedAsWait: true,
            };
            try {
              wrapper.beginAccess(waitFences);
            } catch (e) {
              probe.acceptedAsWait = false;
              throw e;
            }
            if (waitFences) {
              waitFences.forEach((state) => state.fence.destroy());
              waitFences = undefined;
            }
            const encoder = device.createCommandEncoder();
            const pass = encoder.beginRenderPass({
              colorAttachments: [
                {
                  view: ctx.getCurrentTexture().createView(),
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
            const done = wrapper.endAccess();
            if (done !== null) {
              probe.returned = true;
              probe.brand = done.fence.__brand;
              probe.signaledValue = done.signaledValue.toString();
              const info = done.fence.export();
              probe.type = info.type;
              // Re-import the exported handle, as a native module handing the
              // fence back would, and wait on it next time.
              const reimported = device.importSharedFence({
                type: info.type,
                handle: info.handle,
              });
              waitFences = [
                { fence: reimported, signaledValue: done.signaledValue },
              ];
              done.fence.destroy();
            }
            fences.push(probe);
            return canvas.getImageData().then((image: BitmapData) => {
              const pixels = readPixels(image);
              left.push(pixels.left);
              right.push(pixels.right);
              if (index < 2) {
                return accessAndDraw(index + 1);
              }
              return undefined;
            });
          };

          return accessAndDraw(0).then(() => {
            if (waitFences) {
              waitFences.forEach((state) => state.fence.destroy());
            }
            const wrapperBrand = wrapper.__brand;
            const externalTextureBrand = wrapper.externalTexture.__brand;
            wrapper.destroy();
            frame.release();
            return {
              kind: "ok" as const,
              wrapperBrand,
              externalTextureBrand,
              left,
              right,
              fences,
            };
          });
        } catch (e) {
          return {
            kind: "fail",
            reason: `${(e as Error).message ?? e}`,
          };
        }
      },
    );

    if (result.kind === "skip") {
      console.log(`NativeBufferWrapper: skipping (${result.reason})`);
      return;
    }
    if (result.kind === "fail") {
      throw new Error(`NativeBufferWrapper: ${result.reason}`);
    }
    expect(result.wrapperBrand).toBe("GPUNativeBufferExternalTexture");
    expect(result.externalTextureBrand).toBe("GPUExternalTexture");
    expect(result.left).toHaveLength(3);
    expect(result.right).toHaveLength(3);
    // The YUV test frame is BT.709 limited range, red on the left half and
    // green on the right; every access converts it the same way. 8-bit YCbCr
    // quantization and the transfer function round trip leave a few levels
    // of slack.
    result.left.forEach((pixel) => expectColor(pixel, [255, 0, 0], 8));
    result.right.forEach((pixel) => expectColor(pixel, [0, 255, 0], 8));
    // Each access draws, so Dawn produces a fence for it: one per endAccess(),
    // of the device's native kind, which the next access accepts as a wait
    // fence.
    expect(result.fences).toHaveLength(3);
    result.fences.forEach((probe) => {
      expect(probe.returned).toBe(true);
      expect(probe.brand).toBe("GPUSharedFence");
      expect(["sync-fd", "mtl-shared-event"]).toContain(probe.type);
      expect(probe.acceptedAsWait).toBe(true);
    });
  });

  it("samples a color buffer imported once as a regular texture across accesses", async () => {
    const result = await client.eval<Record<string, never>, RgbResult>(
      ({ device, gpu, ctx, canvas }) => {
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
        try {
          const frame = RNWebGPU.createTestVideoFrame(256, 256);
          const wrapper = device.createNativeBufferTexture(frame, {
            usage: GPUTextureUsage.TEXTURE_BINDING,
            label: "rgb-wrapper",
          });
          const module = device.createShaderModule({
            code: /* wgsl */ `
              struct VsOut {
                @builtin(position) position: vec4f,
                @location(0) uv: vec2f,
              };

              @vertex fn vs(@builtin(vertex_index) vid: u32) -> VsOut {
                var positions = array<vec2f, 3>(
                  vec2f(-1.0, -3.0),
                  vec2f(-1.0,  1.0),
                  vec2f( 3.0,  1.0),
                );
                var uvs = array<vec2f, 3>(
                  vec2f(0.0, 2.0),
                  vec2f(0.0, 0.0),
                  vec2f(2.0, 0.0),
                );
                var out: VsOut;
                out.position = vec4f(positions[vid], 0.0, 1.0);
                out.uv = uvs[vid];
                return out;
              }

              @group(0) @binding(0) var srcTex: texture_2d<f32>;
              @group(0) @binding(1) var srcSampler: sampler;

              @fragment fn fs(in: VsOut) -> @location(0) vec4f {
                return textureSample(srcTex, srcSampler, in.uv);
              }
            `,
          });
          const pipeline = device.createRenderPipeline({
            layout: "auto",
            vertex: { module, entryPoint: "vs" },
            fragment: {
              module,
              entryPoint: "fs",
              targets: [{ format: gpu.getPreferredCanvasFormat() }],
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
              { binding: 0, resource: wrapper.texture.createView() },
              { binding: 1, resource: sampler },
            ],
          });
          const draw = () => {
            wrapper.beginAccess();
            const encoder = device.createCommandEncoder();
            const pass = encoder.beginRenderPass({
              colorAttachments: [
                {
                  view: ctx.getCurrentTexture().createView(),
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
            const done = wrapper.endAccess();
            if (done !== null) {
              done.fence.destroy();
            }
          };
          // Two accesses of the one import; the second one is the image
          // compared below.
          draw();
          return canvas.getImageData().then(() => {
            draw();
            return canvas.getImageData().then((image: BitmapData) => {
              const wrapperBrand = wrapper.__brand;
              const textureBrand = wrapper.texture.__brand;
              wrapper.destroy();
              frame.release();
              return {
                kind: "ok" as const,
                wrapperBrand,
                textureBrand,
                ...image,
              };
            });
          });
        } catch (e) {
          return {
            kind: "fail",
            reason: `${(e as Error).message ?? e}`,
          };
        }
      },
    );

    if (result.kind === "skip") {
      console.log(`NativeBufferWrapper: skipping (${result.reason})`);
      return;
    }
    if (result.kind === "fail") {
      throw new Error(`NativeBufferWrapper: ${result.reason}`);
    }
    expect(result.wrapperBrand).toBe("GPUNativeBufferTexture");
    expect(result.textureBrand).toBe("GPUTexture");
    // The same frame and shader as the SharedTextureMemory spec: the same
    // image.
    const image = encodeImage(result);
    checkImage(image, "snapshots/shared-texture-memory.png");
  });

  it("destroy() reports an open access and refuses later use", async () => {
    const result = await client.eval<Record<string, never>, DestroyResult>(
      ({ device }) => {
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
        try {
          const frame = RNWebGPU.createTestVideoFrame(32, 32);
          const wrapper = device.createNativeBufferTexture(frame, {
            label: "destroyed-wrapper",
          });
          const errors: Promise<GPUError | null>[] = [];

          // Destroying with the access open: a validation error, and the
          // access is ended anyway.
          wrapper.beginAccess();
          device.pushErrorScope("validation");
          wrapper.destroy();
          errors.push(device.popErrorScope());

          // A second destroy does nothing.
          device.pushErrorScope("validation");
          wrapper.destroy();
          errors.push(device.popErrorScope());

          // Using the wrapper afterwards: a validation error, then a throw.
          device.pushErrorScope("validation");
          let beginAfterDestroyThrew = false;
          try {
            wrapper.beginAccess();
          } catch {
            beginAfterDestroyThrew = true;
          }
          errors.push(device.popErrorScope());

          device.pushErrorScope("validation");
          let endAfterDestroyThrew = false;
          try {
            wrapper.endAccess();
          } catch {
            endAfterDestroyThrew = true;
          }
          errors.push(device.popErrorScope());

          // No array destructuring in the function evaluated on the device:
          // Babel's helper for it doesn't exist there.
          return Promise.all(errors).then((popped) => {
            // The wrapper dropped its reference to the frame, which the
            // caller still owns.
            const frameHandleAfterDestroy = frame.handle.toString();
            frame.release();
            const message = (error: GPUError | null) =>
              error === null ? null : error.message;
            return {
              kind: "ok" as const,
              destroyWithOpenAccess: message(popped[0]),
              secondDestroy: message(popped[1]),
              beginAfterDestroy: message(popped[2]),
              beginAfterDestroyThrew,
              endAfterDestroy: message(popped[3]),
              endAfterDestroyThrew,
              frameHandleAfterDestroy,
            };
          });
        } catch (e) {
          return {
            kind: "fail",
            reason: `${(e as Error).message ?? e}`,
          };
        }
      },
    );

    if (result.kind === "skip") {
      console.log(`NativeBufferWrapper: skipping (${result.reason})`);
      return;
    }
    if (result.kind === "fail") {
      throw new Error(`NativeBufferWrapper: ${result.reason}`);
    }
    expect(result.destroyWithOpenAccess).toContain("access still open");
    expect(result.destroyWithOpenAccess).toContain("destroyed-wrapper");
    expect(result.secondDestroy).toBeNull();
    expect(result.beginAfterDestroy).toMatch(/destroyed/i);
    expect(result.beginAfterDestroyThrew).toBe(true);
    expect(result.endAfterDestroy).toMatch(/destroyed/i);
    expect(result.endAfterDestroyThrew).toBe(true);
    expect(result.frameHandleAfterDestroy).not.toBe("0");
  });

  it("rejects a crop outside the frame and relabels the wrapped object", async () => {
    const result = await client.eval<Record<string, never>, DescriptorResult>(
      ({ device }) => {
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
        try {
          const frame = RNWebGPU.createTestVideoFrame(32, 32);
          const attempt = (
            descriptor: GPUNativeBufferExternalTextureDescriptor,
          ) => {
            try {
              device.createNativeBufferExternalTexture(frame, descriptor);
              return null;
            } catch (e) {
              return `${(e as Error).message ?? e}`;
            }
          };
          const negativeOrigin = attempt({ cropOrigin: { x: -10, y: 0 } });
          const hugeSize = attempt({ cropSize: [1e12, 16] });
          const originOutside = attempt({ cropOrigin: { x: 32, y: 0 } });
          const sizeWithoutHeight = attempt({
            // Not a GPUExtent2D: the native side still checks it.
            cropSize: { width: 16 } as unknown as GPUExtent2D,
          });
          const emptySize = attempt({ cropSize: [0, 16] });
          const validCrop = attempt({
            cropOrigin: { x: 8, y: 8 },
            cropSize: { width: 16, height: 16 },
          });

          const texture = device.createNativeBufferTexture(frame, {
            label: "before",
          });
          texture.label = "after";
          const external = device.createNativeBufferExternalTexture(frame, {
            label: "before",
          });
          external.label = "after";
          const probed = {
            kind: "ok" as const,
            negativeOrigin,
            hugeSize,
            originOutside,
            sizeWithoutHeight,
            emptySize,
            validCrop,
            textureLabel: texture.texture.label,
            externalTextureLabel: external.externalTexture.label,
            acquireFenceFdType:
              frame.acquireFenceFd === null
                ? "null"
                : typeof frame.acquireFenceFd,
          };
          texture.destroy();
          external.destroy();
          frame.release();
          return probed;
        } catch (e) {
          return {
            kind: "fail",
            reason: `${(e as Error).message ?? e}`,
          };
        }
      },
    );

    if (result.kind === "skip") {
      console.log(`NativeBufferWrapper: skipping (${result.reason})`);
      return;
    }
    if (result.kind === "fail") {
      throw new Error(`NativeBufferWrapper: ${result.reason}`);
    }
    expect(result.negativeOrigin).toContain("cropOrigin.x must be in [0, 32]");
    expect(result.hugeSize).toContain("cropSize.width must be in [0, 32]");
    expect(result.originOutside).toContain("is outside the 32x32 frame");
    expect(result.sizeWithoutHeight).toContain("width and height");
    expect(result.emptySize).toContain("cropSize is empty");
    expect(result.validCrop).toBeNull();
    expect(result.textureLabel).toBe("after");
    expect(result.externalTextureLabel).toBe("after");
    expect(["null", "bigint"]).toContain(result.acquireFenceFdType);
  });
});
