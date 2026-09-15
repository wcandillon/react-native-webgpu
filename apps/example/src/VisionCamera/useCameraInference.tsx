import type React from "react";
import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import type { CameraPosition } from "react-native-vision-camera";
import { createSynchronizable } from "react-native-worklets";

import { CAMERA_BLIT_SHADER, ORIENTATION_INDEX } from "./orientation";
import {
  useWebGPUCamera,
  type WebGPUCameraFrameInfo,
  type WebGPUCameraSetupInfo,
} from "./useWebGPUCamera";

// Camera + tfjs building block shared by the ML demos.
//
// Threading model
// ---------------
// Camera frames arrive on Vision Camera's worklet runtime. There we blit an
// upright copy of the frame into a small square rgba8 texture (the model
// input) and then hand control to the demo's own render worklet. tfjs only
// runs on the main JS runtime, so a loop there copies the input texture into
// a mappable buffer, reads it back and calls the demo's `inference` with the
// RGB bytes. Demos publish their results to the render worklet however they
// like (a worklets Synchronizable for boxes, queue.writeTexture for masks).
//
// Both runtimes therefore drive the same GPUDevice concurrently. Dawn
// devices are not thread-safe by default; this feature makes every device
// call take Dawn's internal lock.
const SYNC_FEATURE = "implicit-device-synchronization" as GPUFeatureName;

export interface UprightFrameInfo {
  width: number;
  height: number;
}

export interface CameraInferenceFrameInfo<
  TState,
> extends WebGPUCameraFrameInfo<TState> {
  // Shader-ready orientation (see orientation.ts).
  rotation: number;
  mirror: number;
  uprightWidth: number;
  uprightHeight: number;
  // Upright blit helpers. The uniform already holds this frame's rotation
  // and mirror flags, so a demo can copy the upright picture into its own
  // texture with one extra render pass.
  blitPipeline: GPURenderPipeline;
  blitUniformBuffer: GPUBuffer;
  sampler: GPUSampler;
}

export interface UseCameraInferenceOptions<TState> {
  // Side of the square model input texture. Must be a multiple of 64 so
  // the readback rows satisfy the 256-byte bytesPerRow alignment.
  inputSize: number;
  cameraPosition?: CameraPosition;
  requiredFeatures?: GPUFeatureName[];
  // Build the demo's pipeline state on the main thread once the device and
  // canvas are ready (see useWebGPUCamera for what may be returned).
  setup: (info: WebGPUCameraSetupInfo) => TState | Promise<TState>;
  // Per-frame worklet, called after the model input blit.
  render?: (frame: CameraInferenceFrameInfo<TState>) => void;
  // Main-thread inference, called with the latest model input as tightly
  // packed RGB bytes. The next readback starts once the promise resolves.
  inference: (rgb: Uint8Array, size: number) => Promise<void>;
}

export interface UseCameraInferenceResult {
  element: React.ReactElement;
  device: GPUDevice | null;
  error: string | null;
  // Upright frame size as last seen by the worklet (zeros until the first
  // frame). Lets main-thread code lay out overlays in the shaders' space.
  getFrameInfo: () => UprightFrameInfo;
}

interface HookState<TState> {
  inner: TState;
  blitPipeline: GPURenderPipeline;
  blitUniformBuffer: GPUBuffer;
  sampler: GPUSampler;
  inputView: GPUTextureView;
}

const nextFrame = () =>
  new Promise<void>((resolve) => requestAnimationFrame(() => resolve()));

interface ReadbackResources {
  device: GPUDevice;
  inputTex: GPUTexture;
  readBuffer: GPUBuffer;
}

export const useCameraInference = <TState,>(
  options: UseCameraInferenceOptions<TState>,
): UseCameraInferenceResult => {
  const {
    inputSize,
    cameraPosition,
    requiredFeatures,
    setup,
    render,
    inference,
  } = options;
  if ((inputSize * 4) % 256 !== 0) {
    throw new Error("inputSize must be a multiple of 64");
  }
  const bytesPerRow = inputSize * 4;

  const frameInfo = useMemo(
    () => createSynchronizable<UprightFrameInfo>({ width: 0, height: 0 }),
    [],
  );
  const [resources, setResources] = useState<ReadbackResources | null>(null);
  const [error, setError] = useState<string | null>(null);

  // Inference is read through a ref so demos can close over React state
  // without restarting the readback loop.
  const inferenceRef = useRef(inference);
  useEffect(() => {
    inferenceRef.current = inference;
  }, [inference]);

  const features = useMemo(
    () => [SYNC_FEATURE, ...(requiredFeatures ?? [])],
    [requiredFeatures],
  );

  const { element, device } = useWebGPUCamera<HookState<TState>>({
    requiredFeatures: features,
    cameraPosition,
    setup: async (info) => {
      const { device: gpu } = info;
      const module = gpu.createShaderModule({ code: CAMERA_BLIT_SHADER });
      const blitPipeline = gpu.createRenderPipeline({
        layout: "auto",
        vertex: { module, entryPoint: "vs_main" },
        fragment: {
          module,
          entryPoint: "fs_main",
          targets: [{ format: "rgba8unorm" }],
        },
        primitive: { topology: "triangle-list" },
      });
      const sampler = gpu.createSampler({
        magFilter: "linear",
        minFilter: "linear",
      });
      const blitUniformBuffer = gpu.createBuffer({
        size: 16,
        usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
      });
      const inputTex = gpu.createTexture({
        size: [inputSize, inputSize],
        format: "rgba8unorm",
        usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.COPY_SRC,
      });
      const readBuffer = gpu.createBuffer({
        size: bytesPerRow * inputSize,
        usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
      });
      const inner = await setup(info);
      setResources({ device: gpu, inputTex, readBuffer });
      return {
        inner,
        blitPipeline,
        blitUniformBuffer,
        sampler,
        inputView: inputTex.createView(),
      };
    },
    render: (frame) => {
      "worklet";
      const {
        device: gpu,
        externalTexture,
        frameWidth,
        frameHeight,
        orientation,
        isMirrored,
        pipelineState,
      } = frame;
      const { inner, blitPipeline, blitUniformBuffer, sampler, inputView } =
        pipelineState;
      const rotation = ORIENTATION_INDEX[orientation];
      const mirror = isMirrored ? 1 : 0;
      const sideways = rotation === 1 || rotation === 3;
      const uprightWidth = sideways ? frameHeight : frameWidth;
      const uprightHeight = sideways ? frameWidth : frameHeight;
      const last = frameInfo.getDirty();
      if (last.width !== uprightWidth || last.height !== uprightHeight) {
        frameInfo.setBlocking({ width: uprightWidth, height: uprightHeight });
      }

      // Model input: upright blit into the square input texture. The main
      // thread copies this texture out whenever it is ready for a new
      // inference; the copy is a queue operation so it always lands after a
      // complete blit.
      gpu.queue.writeBuffer(
        blitUniformBuffer,
        0,
        new Uint32Array([rotation, mirror, 0, 0]),
      );
      const bindGroup = gpu.createBindGroup({
        layout: blitPipeline.getBindGroupLayout(0),
        entries: [
          { binding: 0, resource: externalTexture },
          { binding: 1, resource: sampler },
          { binding: 2, resource: { buffer: blitUniformBuffer } },
        ],
      });
      const encoder = gpu.createCommandEncoder();
      const pass = encoder.beginRenderPass({
        colorAttachments: [
          {
            view: inputView,
            clearValue: { r: 0, g: 0, b: 0, a: 1 },
            loadOp: "clear",
            storeOp: "store",
          },
        ],
      });
      pass.setPipeline(blitPipeline);
      pass.setBindGroup(0, bindGroup);
      pass.draw(3);
      pass.end();
      gpu.queue.submit([encoder.finish()]);

      if (render) {
        render({
          ...frame,
          pipelineState: inner,
          rotation,
          mirror,
          uprightWidth,
          uprightHeight,
          blitPipeline,
          blitUniformBuffer,
          sampler,
        });
      }
    },
  });

  // Main-thread readback loop: copy -> map -> RGB -> demo inference.
  useEffect(() => {
    if (!resources) {
      return;
    }
    const { device: gpu, inputTex, readBuffer } = resources;
    let disposed = false;
    (async () => {
      try {
        const rgb = new Uint8Array(inputSize * inputSize * 3);
        while (!disposed) {
          const encoder = gpu.createCommandEncoder();
          encoder.copyTextureToBuffer(
            { texture: inputTex },
            { buffer: readBuffer, bytesPerRow },
            [inputSize, inputSize],
          );
          gpu.queue.submit([encoder.finish()]);
          await readBuffer.mapAsync(GPUMapMode.READ);
          const rgba = new Uint8Array(readBuffer.getMappedRange());
          for (let i = 0, j = 0; i < rgba.length; i += 4, j += 3) {
            rgb[j] = rgba[i];
            rgb[j + 1] = rgba[i + 1];
            rgb[j + 2] = rgba[i + 2];
          }
          readBuffer.unmap();
          if (disposed) {
            break;
          }
          await inferenceRef.current(rgb, inputSize);
          // Yield to a real frame before the next readback. On iOS
          // react-native-wgpu resolves mapAsync by re-queueing a microtask
          // until the GPU is done, and every await above resumes from a
          // microtask too, so without this the JS thread never drains its
          // microtask queue: requestAnimationFrame, timers and React updates
          // all starve. Demos that present from the main thread (three.js)
          // freeze without it.
          await nextFrame();
        }
      } catch (e) {
        if (!disposed) {
          console.warn("[useCameraInference] inference failed: " + String(e));
          setError(String(e));
        }
      } finally {
        // Only reached once the loop is out of the buffer, so it is safe to
        // release it here even if the unmount raced a pending map.
        readBuffer.destroy();
      }
    })();
    return () => {
      disposed = true;
    };
  }, [resources, inputSize, bytesPerRow]);

  const getFrameInfo = useCallback(() => frameInfo.getBlocking(), [frameInfo]);

  return { element, device, error, getFrameInfo };
};
