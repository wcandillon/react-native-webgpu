import React, { useEffect, useMemo, useState } from "react";
import { StyleSheet, Text, View } from "react-native";
import { createSynchronizable } from "react-native-worklets";
import * as tf from "@tensorflow/tfjs";
import "@tensorflow/tfjs-backend-webgpu";
import * as faceDetection from "@tensorflow-models/face-detection";

import { PlatformReactNative } from "../Tensorflow/Platform";
import { useWebGPUCamera } from "../VisionCamera/useWebGPUCamera";

import { DETECT_SHADER, SHADER } from "./shader";

// tfjs uses this for fetch / encode / decode / timing in non-browser
// environments. Same Platform impl as the Tensorflow demo.
tf.setPlatform("react-native", new PlatformReactNative());

// Threading model
// ---------------
// Camera frames arrive on Vision Camera's worklet runtime, where we draw the
// live picture plus the ring overlay and blit a small upright copy of the
// frame into an offscreen texture. BlazeFace (tfjs) only runs on the main
// JS runtime, so a loop there copies that texture into a mappable buffer,
// reads it back, runs the detector and publishes the boxes through a
// worklets Synchronizable that the render worklet reads every frame.
//
// Both runtimes therefore drive the same GPUDevice concurrently. Dawn
// devices are not thread-safe by default; this feature makes every device
// call take Dawn's internal lock.
const REQUIRED_FEATURES: GPUFeatureName[] = [
  "implicit-device-synchronization" as GPUFeatureName,
];

// BlazeFace's "short" variant accepts a 128x128 input but we feed it at
// 192x192 to keep some headroom for the bounding box regression. 192 * 4 =
// 768, a multiple of 256, so the copyTextureToBuffer bytesPerRow constraint
// is satisfied without padding.
const DETECT_SIZE = 192;
const DETECT_BYTES_PER_ROW = DETECT_SIZE * 4;
const MAX_FACES = 8;
const UNIFORM_SIZE = 32 + 16 * MAX_FACES;
const DETECT_UNIFORM_SIZE = 16;

// frame.orientation -> the rotation index the shaders expect.
const ORIENTATION_INDEX = { up: 0, right: 1, down: 2, left: 3 } as const;

interface PipelineState {
  pipeline: GPURenderPipeline;
  sampler: GPUSampler;
  uniformBuffer: GPUBuffer;
  detectPipeline: GPURenderPipeline;
  detectView: GPUTextureView;
  detectUniformBuffer: GPUBuffer;
  startTime: number;
}

// Face boxes in normalized upright-image UV space, packed as
// (xMin, yMin, width, height) per face. Written by the main thread, read by
// the camera worklet.
interface FaceBoxes {
  count: number;
  boxes: number[];
}

interface DetectResources {
  device: GPUDevice;
  detectTex: GPUTexture;
  readBuffer: GPUBuffer;
}

export const FaceDetection = () => {
  const [status, setStatus] = useState("Waiting for camera...");
  const [error, setError] = useState<string | null>(null);
  const [detectResources, setDetectResources] =
    useState<DetectResources | null>(null);

  const faces = useMemo(
    () =>
      createSynchronizable<FaceBoxes>({
        count: 0,
        boxes: new Array<number>(MAX_FACES * 4).fill(0),
      }),
    [],
  );

  const { element } = useWebGPUCamera<PipelineState>({
    requiredFeatures: REQUIRED_FEATURES,
    cameraPosition: "front",
    setup: ({ device, presentationFormat }) => {
      // ----- Display pipeline -------------------------------------------
      const mainModule = device.createShaderModule({ code: SHADER });
      const pipeline = device.createRenderPipeline({
        layout: "auto",
        vertex: { module: mainModule, entryPoint: "vs_main" },
        fragment: {
          module: mainModule,
          entryPoint: "fs_main",
          targets: [{ format: presentationFormat }],
        },
        primitive: { topology: "triangle-list" },
      });
      const sampler = device.createSampler({
        magFilter: "linear",
        minFilter: "linear",
      });
      const uniformBuffer = device.createBuffer({
        size: UNIFORM_SIZE,
        usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
      });

      // ----- Detection pipeline -----------------------------------------
      // Blit the upright camera image into a 192x192 rgba8 texture every
      // frame. The main thread copies it into a mappable buffer whenever it
      // is ready for another inference.
      const detectModule = device.createShaderModule({ code: DETECT_SHADER });
      const detectPipeline = device.createRenderPipeline({
        layout: "auto",
        vertex: { module: detectModule, entryPoint: "vs_main" },
        fragment: {
          module: detectModule,
          entryPoint: "fs_main",
          targets: [{ format: "rgba8unorm" }],
        },
        primitive: { topology: "triangle-list" },
      });
      const detectTex = device.createTexture({
        size: [DETECT_SIZE, DETECT_SIZE],
        format: "rgba8unorm",
        usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.COPY_SRC,
      });
      const detectUniformBuffer = device.createBuffer({
        size: DETECT_UNIFORM_SIZE,
        usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
      });
      const readBuffer = device.createBuffer({
        size: DETECT_BYTES_PER_ROW * DETECT_SIZE,
        usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
      });
      setDetectResources({ device, detectTex, readBuffer });

      return {
        pipeline,
        sampler,
        uniformBuffer,
        detectPipeline,
        detectView: detectTex.createView(),
        detectUniformBuffer,
        startTime: Date.now(),
      };
    },
    render: ({
      device,
      context,
      externalTexture,
      canvasWidth,
      canvasHeight,
      frameWidth,
      frameHeight,
      orientation,
      isMirrored,
      pipelineState,
    }) => {
      "worklet";
      const {
        pipeline,
        sampler,
        uniformBuffer,
        detectPipeline,
        detectView,
        detectUniformBuffer,
        startTime,
      } = pipelineState;
      const rotation = ORIENTATION_INDEX[orientation];
      const mirror = isMirrored ? 1 : 0;
      const detected = faces.getDirty();

      const uniformData = new ArrayBuffer(UNIFORM_SIZE);
      const uniformF32 = new Float32Array(uniformData);
      const uniformU32 = new Uint32Array(uniformData);
      uniformF32[0] = frameWidth;
      uniformF32[1] = frameHeight;
      uniformF32[2] = canvasWidth;
      uniformF32[3] = canvasHeight;
      uniformU32[4] = detected.count;
      uniformF32[5] = (Date.now() - startTime) / 1000;
      uniformU32[6] = rotation;
      uniformU32[7] = mirror;
      // The faces array starts at byte 32 (index 8 in the F32 view).
      uniformF32.set(detected.boxes, 8);
      device.queue.writeBuffer(uniformBuffer, 0, uniformData);
      device.queue.writeBuffer(
        detectUniformBuffer,
        0,
        new Uint32Array([rotation, mirror, 0, 0]),
      );

      const encoder = device.createCommandEncoder();

      // 1. Upright blit for the detector.
      const detectBindGroup = device.createBindGroup({
        layout: detectPipeline.getBindGroupLayout(0),
        entries: [
          { binding: 0, resource: externalTexture },
          { binding: 1, resource: sampler },
          { binding: 2, resource: { buffer: detectUniformBuffer } },
        ],
      });
      const detectPass = encoder.beginRenderPass({
        colorAttachments: [
          {
            view: detectView,
            clearValue: { r: 0, g: 0, b: 0, a: 1 },
            loadOp: "clear",
            storeOp: "store",
          },
        ],
      });
      detectPass.setPipeline(detectPipeline);
      detectPass.setBindGroup(0, detectBindGroup);
      detectPass.draw(3);
      detectPass.end();

      // 2. Live picture plus ring overlay.
      const bindGroup = device.createBindGroup({
        layout: pipeline.getBindGroupLayout(0),
        entries: [
          { binding: 0, resource: externalTexture },
          { binding: 1, resource: sampler },
          { binding: 2, resource: { buffer: uniformBuffer } },
        ],
      });
      const pass = encoder.beginRenderPass({
        colorAttachments: [
          {
            view: context.getCurrentTexture().createView(),
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
      context.present();
    },
  });

  // Main-thread detection loop: readback -> tensor -> BlazeFace -> publish.
  useEffect(() => {
    if (!detectResources) {
      return;
    }
    const { device, detectTex, readBuffer } = detectResources;
    let disposed = false;

    (async () => {
      try {
        setStatus("Initialising tfjs WebGPU backend...");
        await tf.setBackend("webgpu");
        await tf.ready();
        setStatus("Loading face detector model...");
        const detector = await faceDetection.createDetector(
          faceDetection.SupportedModels.MediaPipeFaceDetector,
          { runtime: "tfjs", modelType: "short" },
        );
        if (disposed) {
          return;
        }
        setStatus("Detecting faces...");

        const rgb = new Uint8Array(DETECT_SIZE * DETECT_SIZE * 3);
        while (!disposed) {
          // The copy is a queue operation, so it lands after whatever blit
          // passes the camera worklet has already submitted and always sees
          // a complete frame. The read buffer is only ever touched here.
          const encoder = device.createCommandEncoder();
          encoder.copyTextureToBuffer(
            { texture: detectTex },
            { buffer: readBuffer, bytesPerRow: DETECT_BYTES_PER_ROW },
            [DETECT_SIZE, DETECT_SIZE],
          );
          device.queue.submit([encoder.finish()]);
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

          const tensor = tf.tensor3d(rgb, [DETECT_SIZE, DETECT_SIZE, 3]);
          const detected = await detector.estimateFaces(tensor, {
            flipHorizontal: false,
          });
          tensor.dispose();
          if (disposed) {
            break;
          }

          const boxes = new Array<number>(MAX_FACES * 4).fill(0);
          const count = Math.min(detected.length, MAX_FACES);
          for (let i = 0; i < count; i++) {
            const b = detected[i].box;
            boxes[i * 4 + 0] = b.xMin / DETECT_SIZE;
            boxes[i * 4 + 1] = b.yMin / DETECT_SIZE;
            boxes[i * 4 + 2] = b.width / DETECT_SIZE;
            boxes[i * 4 + 3] = b.height / DETECT_SIZE;
          }
          faces.setBlocking({ count, boxes });
        }
      } catch (e) {
        if (!disposed) {
          setError(`Face detection failed: ${String(e)}`);
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
  }, [detectResources, faces]);

  return (
    <View style={styles.root}>
      {element}
      <View style={styles.statusBar}>
        <Text style={error ? styles.errorText : styles.statusText}>
          {error ?? status}
        </Text>
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  root: { flex: 1, backgroundColor: "black" },
  statusBar: {
    position: "absolute",
    top: 16,
    left: 16,
    right: 16,
    backgroundColor: "rgba(0,0,0,0.55)",
    paddingHorizontal: 10,
    paddingVertical: 6,
    borderRadius: 6,
  },
  statusText: { color: "white", fontSize: 12 },
  errorText: { color: "#ff6b6b", fontSize: 12 },
});
