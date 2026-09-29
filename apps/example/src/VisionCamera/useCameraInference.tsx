import type React from "react";
import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import type { CameraPosition } from "react-native-vision-camera";
import { createSynchronizable } from "react-native-worklets";
import * as tf from "@tensorflow/tfjs";

import {
  CAMERA_BLIT_SHADER,
  inputPackShader,
  ORIENTATION_INDEX,
} from "./orientation";
import { ensureTfjsWebGPU } from "./tfjs";
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
// runs on the main JS runtime, so a loop there packs that texture into a
// tensor-shaped storage buffer with a compute pass, wraps the buffer as a
// zero-copy tf.Tensor and calls the demo's `inference` with it. Demos
// publish their results to the render worklet however they like (a worklets
// Synchronizable for boxes, a buffer copy for masks).
//
// tfjs runs on the same GPUDevice as everything else (see tfjs.ts), so no
// pixels ever cross to the CPU. Both runtimes therefore drive one device
// concurrently. Dawn devices are not thread-safe by default; this feature
// makes every device call take Dawn's internal lock.
const SYNC_FEATURE = "implicit-device-synchronization" as GPUFeatureName;
const PACK_WORKGROUP = 8;

export type InputDtype = "float32" | "int32";

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
  // Side of the square model input texture.
  inputSize: number;
  // Element type of the input tensor handed to `inference`.
  inputDtype?: InputDtype;
  cameraPosition?: CameraPosition;
  requiredFeatures?: GPUFeatureName[];
  // Build the demo's pipeline state on the main thread once the device,
  // canvas and tfjs backend are ready. Start model loading here.
  setup: (info: WebGPUCameraSetupInfo) => TState | Promise<TState>;
  // Per-frame worklet, called after the model input blit.
  render?: (frame: CameraInferenceFrameInfo<TState>) => void;
  // Main-thread inference, called with the latest model input as a
  // [size, size, 3] tensor living on the GPU. The tensor is disposed after
  // the promise resolves and the next frame is packed only then.
  inference: (input: tf.Tensor3D, size: number) => Promise<void>;
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

interface InferenceResources {
  device: GPUDevice;
  inputTex: GPUTexture;
  inputBuffer: GPUBuffer;
  packPipeline: GPUComputePipeline;
  packBindGroup: GPUBindGroup;
}

const nextFrame = () =>
  new Promise<void>((resolve) => requestAnimationFrame(() => resolve()));

export const useCameraInference = <TState,>(
  options: UseCameraInferenceOptions<TState>,
): UseCameraInferenceResult => {
  const {
    inputSize,
    inputDtype = "float32",
    cameraPosition,
    requiredFeatures,
    setup,
    render,
    inference,
  } = options;

  const frameInfo = useMemo(
    () => createSynchronizable<UprightFrameInfo>({ width: 0, height: 0 }),
    [],
  );
  const [resources, setResources] = useState<InferenceResources | null>(null);
  const [error, setError] = useState<string | null>(null);

  // Inference is read through a ref so demos can close over React state
  // without restarting the inference loop.
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
      const { device: gpu, adapter } = info;
      await ensureTfjsWebGPU(gpu, adapter);

      const blitModule = gpu.createShaderModule({ code: CAMERA_BLIT_SHADER });
      const blitPipeline = gpu.createRenderPipeline({
        layout: "auto",
        vertex: { module: blitModule, entryPoint: "vs_main" },
        fragment: {
          module: blitModule,
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
        usage:
          GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.TEXTURE_BINDING,
      });

      // Tensor-shaped copy of the input. STORAGE | COPY_SRC is what tfjs
      // requires to wrap a buffer without copying it.
      const inputBuffer = gpu.createBuffer({
        size: inputSize * inputSize * 3 * 4,
        usage:
          GPUBufferUsage.STORAGE |
          GPUBufferUsage.COPY_SRC |
          GPUBufferUsage.COPY_DST,
      });
      const packModule = gpu.createShaderModule({
        code: inputPackShader(inputDtype),
      });
      const packPipeline = gpu.createComputePipeline({
        layout: "auto",
        compute: { module: packModule, entryPoint: "main" },
      });
      const packBindGroup = gpu.createBindGroup({
        layout: packPipeline.getBindGroupLayout(0),
        entries: [
          { binding: 0, resource: inputTex.createView() },
          { binding: 1, resource: { buffer: inputBuffer } },
        ],
      });

      const inner = await setup(info);
      setResources({
        device: gpu,
        inputTex,
        inputBuffer,
        packPipeline,
        packBindGroup,
      });
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
      // thread packs this texture into the tensor buffer whenever it is
      // ready for a new inference; the pack is a queue operation so it
      // always sees a complete blit.
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

  // Main-thread inference loop: pack -> zero-copy tensor -> demo inference.
  useEffect(() => {
    if (!resources) {
      return;
    }
    const {
      device: gpu,
      inputTex,
      inputBuffer,
      packPipeline,
      packBindGroup,
    } = resources;
    const groups = Math.ceil(inputSize / PACK_WORKGROUP);
    let disposed = false;
    (async () => {
      try {
        while (!disposed) {
          const encoder = gpu.createCommandEncoder();
          const pass = encoder.beginComputePass();
          pass.setPipeline(packPipeline);
          pass.setBindGroup(0, packBindGroup);
          pass.dispatchWorkgroups(groups, groups);
          pass.end();
          gpu.queue.submit([encoder.finish()]);

          // zeroCopy: the tensor binds our buffer directly and disposing it
          // leaves the buffer alone. Nothing writes the buffer again until
          // the demo is done with the tensor.
          const input = tf.tensor(
            { buffer: inputBuffer, zeroCopy: true },
            [inputSize, inputSize, 3],
            inputDtype,
          ) as tf.Tensor3D;
          try {
            await inferenceRef.current(input, inputSize);
          } finally {
            input.dispose();
          }
          if (disposed) {
            break;
          }
          // Yield to a real frame before the next inference. On iOS
          // react-native-wgpu resolves mapAsync (which tfjs uses to read
          // results) by re-queueing a microtask until the GPU is done, and
          // every await above resumes from a microtask too, so without this
          // the JS thread never drains its microtask queue:
          // requestAnimationFrame, timers and React updates all starve.
          await nextFrame();
        }
      } catch (e) {
        if (!disposed) {
          console.warn("[useCameraInference] inference failed: " + String(e));
          setError(String(e));
        }
      } finally {
        // Only reached once no tensor references the buffer any more.
        inputBuffer.destroy();
        inputTex.destroy();
      }
    })();
    return () => {
      disposed = true;
    };
  }, [resources, inputSize, inputDtype]);

  const getFrameInfo = useCallback(() => frameInfo.getBlocking(), [frameInfo]);

  return { element, device, error, getFrameInfo };
};
