/* eslint-disable @typescript-eslint/no-explicit-any */
import fs from "fs";
import path from "path";

import { client } from "./setup";

// A 32x32 PNG, decoded on the main runtime for the ImageBitmap round trip.
const assetPath = path.resolve(__dirname, "./assets/alpha-gradient.png");

// WebGPU objects are plain JS objects with jsi::NativeState and a per-runtime
// prototype, which worklets cannot copy by itself. registerWebGPUForReanimated
// registers a custom serializer whose native half is the
// __webgpuWorkletHelpers HostObject (WebGPUWorkletHelpers in
// cpp/jsi/NativeObject.h). These specs exercise that helper directly and
// through a real worklet runtime. They only run on the native app: the web and
// node runners have no helper and return null, which skips the assertions.
describe("Worklet serialization", () => {
  it("claims WebGPU objects and nothing else", async () => {
    const result = await client.eval(({ device, workletSerialization }) => {
      const helpers = (globalThis as any).__webgpuWorkletHelpers;
      if (!helpers || !workletSerialization) {
        return null;
      }
      const buffer = device.createBuffer({
        size: 256,
        usage: GPUBufferUsage.COPY_DST,
      });
      // A JS object whose prototype carries Symbol.toStringTag the way WebGPU
      // prototypes do, but without native state: the shape of a Skia object or
      // of any tagged class instance.
      const taggedProto = {};
      Object.defineProperty(taggedProto, Symbol.toStringTag, {
        value: "GPUBuffer",
      });
      const tagged = Object.create(taggedProto);
      // Inherits every GPUBuffer method through the prototype chain but has
      // no native state of its own.
      const inheriting = Object.create(buffer);
      let boxInheritingThrows = false;
      try {
        helpers.box(inheriting);
      } catch {
        boxInheritingThrows = true;
      }
      const boxed = helpers.box(buffer);
      const results = {
        device: helpers.isNativeObject(device),
        buffer: helpers.isNativeObject(buffer),
        queue: helpers.isNativeObject(device.queue),
        tagged: helpers.isNativeObject(tagged),
        inheriting: helpers.isNativeObject(inheriting),
        foreignNativeState: helpers.isNativeObject(
          workletSerialization.foreignNativeStateObject(),
        ),
        plain: helpers.isNativeObject({}),
        primitives:
          helpers.isNativeObject(null) ||
          helpers.isNativeObject(undefined) ||
          helpers.isNativeObject(42) ||
          helpers.isNativeObject("GPUBuffer"),
        boxInheritingThrows,
        brand: boxed.__brand,
        unboxesToSameObject: boxed.unbox() === buffer,
      };
      buffer.destroy();
      return results;
    });
    if (result === null) {
      return;
    }
    expect(result).toEqual({
      device: true,
      buffer: true,
      queue: true,
      tagged: false,
      inheriting: false,
      foreignNativeState: false,
      plain: false,
      primitives: false,
      boxInheritingThrows: true,
      brand: "GPUBuffer",
      unboxesToSameObject: true,
    });
  });

  it("round-trips a buffer through a worklet runtime", async () => {
    const result = await client.eval<
      Record<string, unknown>,
      {
        size: number;
        label: string;
        isGPUBuffer: boolean;
        sameObjectBack: boolean;
      } | null
    >(({ device, workletSerialization }) => {
      if (!workletSerialization) {
        return null;
      }
      const buffer = device.createBuffer({
        size: 512,
        usage: GPUBufferUsage.COPY_DST,
        label: "round-trip",
      });
      return workletSerialization.roundTripBuffer(buffer).then((r) => {
        buffer.destroy();
        return r;
      });
    });
    if (result === null) {
      return;
    }
    expect(result).toEqual({
      size: 512,
      label: "round-trip",
      isGPUBuffer: true,
      sameObjectBack: true,
    });
  });

  it("round-trips an ImageBitmap through a worklet runtime", async () => {
    // ImageBitmap was missing from RNWebGPUManager::installConstructors, so
    // its brand was never registered and unboxing it on a worklet runtime
    // threw "No native class registered for brand: ImageBitmap".
    const pngData = Array.from(fs.readFileSync(assetPath));
    const result = await client.eval<
      { pngData: number[] },
      {
        width: number;
        height: number;
        isImageBitmap: boolean;
        sameObjectBack: boolean;
        error?: string;
      } | null
    >(
      ({ workletSerialization, pngData: encodedPng }) => {
        if (!workletSerialization) {
          return null;
        }
        const bytes = new Uint8Array(encodedPng);
        return createImageBitmap(bytes.buffer).then((bitmap) =>
          workletSerialization.roundTripImageBitmap(bitmap),
        );
      },
      { pngData },
    );
    if (result === null) {
      return;
    }
    expect(result).toEqual({
      width: 32,
      height: 32,
      isImageBitmap: true,
      sameObjectBack: true,
    });
  });

  it("installs every class constructor on a worklet runtime", async () => {
    // Issue #564: three.js checks `pass instanceof GPURenderBundleEncoder` on
    // every render. Unboxing installs only the classes of the objects that
    // cross, so a class nothing ever instantiated on the runtime had no
    // global there and the check threw a ReferenceError.
    const result = await client.eval<
      Record<string, unknown>,
      {
        renderBundleEncoder: string;
        passIsComputePassEncoder: boolean;
        passIsRenderBundleEncoder: boolean;
        stableIdentity: boolean;
        error?: string;
      } | null
    >(({ device, workletSerialization }) => {
      if (!workletSerialization) {
        return null;
      }
      return workletSerialization.installsConstructors(device);
    });
    if (result === null) {
      return;
    }
    expect(result).toEqual({
      renderBundleEncoder: "function",
      passIsComputePassEncoder: true,
      passIsRenderBundleEncoder: false,
      stableIdentity: true,
    });
  });

  it("leaves an object inheriting from a WebGPU prototype to worklets", async () => {
    const result = await client.eval<
      Record<string, unknown>,
      { threw: boolean; message: string; typeOnWorklet: string } | null
    >(({ device, workletSerialization }) => {
      if (!workletSerialization) {
        return null;
      }
      const buffer = device.createBuffer({
        size: 256,
        usage: GPUBufferUsage.COPY_DST,
      });
      // Not a WebGPU object: the serializer must not claim it and then fail
      // to box it, which would make capturing the worklet throw.
      return workletSerialization
        .captureForeign(Object.create(buffer))
        .then((r) => {
          buffer.destroy();
          return r;
        });
    });
    if (result === null) {
      return;
    }
    expect(result.threw).toBe(false);
  });
});
