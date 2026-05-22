/* eslint-disable @typescript-eslint/no-shadow */
import React, { useEffect, useRef, useState } from "react";
import {
  Image,
  PixelRatio,
  Platform,
  StyleSheet,
  Text,
  View,
} from "react-native";
import {
  Canvas,
  useCanvasRef,
  useDevice,
  type NativeCanvas,
  type VideoFrame,
} from "react-native-wgpu";
import * as tf from "@tensorflow/tfjs";
import "@tensorflow/tfjs-backend-webgpu";
import * as faceDetection from "@tensorflow-models/face-detection";

import { PlatformReactNative } from "../Tensorflow/Platform";

import { DETECT_SHADER, SHADER } from "./shader";

// tfjs uses this for fetch / encode / decode / timing in non-browser
// environments. Same Platform impl as the Tensorflow demo.
tf.setPlatform("react-native", new PlatformReactNative());

// Same feature set as the YUV ExternalTexture demo: shared-texture-memory
// + multi-planar formats so importExternalTexture can sample the NV12
// surface AVPlayer hands us.
const REQUIRED_FEATURES: GPUFeatureName[] = [
  "rnwebgpu/shared-texture-memory" as GPUFeatureName,
  "dawn-multi-planar-formats" as GPUFeatureName,
];

// Bundled clip that ships with the example app (same asset as the
// ExternalTexture demo). Swap to the VisionCamera live feed when you're
// ready to graduate off file playback.
const VIDEO_URL = Image.resolveAssetSource(
  require("../assets/clip.mov"),
).uri;

// BlazeFace's "short" variant accepts a 128x128 input but we feed it at
// 192x192 to keep some headroom for the bounding box regression. 192 * 4 =
// 768, a multiple of 256, so the copyTextureToBuffer bytesPerRow constraint
// is satisfied without padding.
const DETECT_SIZE = 192;
const MAX_FACES = 8;
const UNIFORM_SIZE = 32 + 16 * MAX_FACES;

export const FaceDetection = () => {
  const ref = useCanvasRef();
  const [error, setError] = useState<string | null>(null);
  const [status, setStatus] = useState("Initialising...");
  const rafRef = useRef<number | null>(null);
  const { device, adapter } = useDevice(undefined, {
    requiredFeatures: REQUIRED_FEATURES,
  });

  useEffect(() => {
    if (!device) {
      return;
    }
    const missing = REQUIRED_FEATURES.filter((f) => !device.features.has(f));
    if (missing.length > 0) {
      setError(
        `Device is missing required features [${missing.join(", ")}]. ` +
          `Adapter supports: ${
            adapter
              ? [...adapter.features]
                  .filter((f) => f.toString().startsWith("shared-"))
                  .join(", ") || "none"
              : "n/a"
          }`,
      );
      return;
    }
    if (Platform.OS !== "ios" && Platform.OS !== "macos") {
      setError(
        "Face detection demo currently relies on createVideoPlayer, which " +
          "is iOS/macOS-only today. Android support is pending.",
      );
      return;
    }

    const context = ref.current?.getContext("webgpu");
    if (!context) {
      return;
    }
    const canvas = context.canvas as unknown as NativeCanvas;
    canvas.width = canvas.clientWidth * PixelRatio.get();
    canvas.height = canvas.clientHeight * PixelRatio.get();
    const presentationFormat = navigator.gpu.getPreferredCanvasFormat();
    context.configure({
      device,
      format: presentationFormat,
      alphaMode: "premultiplied",
    });

    const player = RNWebGPU.createVideoPlayer(VIDEO_URL, "nv12");
    player.play();

    // ----- Display pipeline ---------------------------------------------
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
    const uniformData = new ArrayBuffer(UNIFORM_SIZE);
    const uniformF32 = new Float32Array(uniformData);
    const uniformU32 = new Uint32Array(uniformData);

    // ----- Detection pipeline -------------------------------------------
    // Render the video into a 192x192 rgba8 texture, copy it to a mappable
    // buffer, build a tf.Tensor3D from the bytes, and ship that to BlazeFace.
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
      usage:
        GPUTextureUsage.RENDER_ATTACHMENT |
        GPUTextureUsage.TEXTURE_BINDING |
        GPUTextureUsage.COPY_SRC,
    });
    const detectBytesPerRow = DETECT_SIZE * 4;
    const detectReadBuffer = device.createBuffer({
      size: detectBytesPerRow * DETECT_SIZE,
      usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
    });

    // Video frames flow into here from the rAF loop. Detection reads the
    // latest one whenever it's ready for another inference.
    let currentFrame: VideoFrame | null = null;
    let detector: faceDetection.FaceDetector | null = null;
    let detectionBusy = false;
    let disposed = false;

    // Face boxes in normalized texture-UV space (xMin, yMin, width, height).
    let faces = new Float32Array(MAX_FACES * 4);
    let numFaces = 0;

    (async () => {
      try {
        setStatus("Initialising tfjs WebGPU backend...");
        await tf.setBackend("webgpu");
        await tf.ready();
        setStatus("Loading face detector model...");
        detector = await faceDetection.createDetector(
          faceDetection.SupportedModels.MediaPipeFaceDetector,
          { runtime: "tfjs", modelType: "short" },
        );
        if (!disposed) {
          setStatus("Detecting faces...");
        }
      } catch (e) {
        if (!disposed) {
          setError(`Failed to load detector: ${String(e)}`);
        }
      }
    })();

    const runDetection = async () => {
      if (disposed || !detector || detectionBusy || !currentFrame) {
        return;
      }
      detectionBusy = true;
      try {
        // The external texture is single-use per command encoder; build a
        // fresh one for the detection pass so we don't fight the display
        // pipeline for it.
        const externalTex = device.importExternalTexture({
          source: currentFrame,
          label: "video-detect",
        });
        const bindGroup = device.createBindGroup({
          layout: detectPipeline.getBindGroupLayout(0),
          entries: [
            { binding: 0, resource: externalTex },
            { binding: 1, resource: sampler },
          ],
        });
        const encoder = device.createCommandEncoder();
        const pass = encoder.beginRenderPass({
          colorAttachments: [
            {
              view: detectTex.createView(),
              clearValue: { r: 0, g: 0, b: 0, a: 1 },
              loadOp: "clear",
              storeOp: "store",
            },
          ],
        });
        pass.setPipeline(detectPipeline);
        pass.setBindGroup(0, bindGroup);
        pass.draw(3);
        pass.end();
        encoder.copyTextureToBuffer(
          { texture: detectTex },
          { buffer: detectReadBuffer, bytesPerRow: detectBytesPerRow },
          [DETECT_SIZE, DETECT_SIZE],
        );
        device.queue.submit([encoder.finish()]);

        await detectReadBuffer.mapAsync(GPUMapMode.READ);
        if (disposed) {
          detectReadBuffer.unmap();
          return;
        }
        const rgba = new Uint8Array(
          detectReadBuffer.getMappedRange(),
        ).slice();
        detectReadBuffer.unmap();

        // BlazeFace via tfjs expects HxWx3 float32 with values in [0, 255]
        // — matches what tf.browser.fromPixels would have produced.
        const rgb = new Float32Array(DETECT_SIZE * DETECT_SIZE * 3);
        for (let i = 0, j = 0; i < rgba.length; i += 4, j += 3) {
          rgb[j] = rgba[i];
          rgb[j + 1] = rgba[i + 1];
          rgb[j + 2] = rgba[i + 2];
        }
        const tensor = tf.tensor3d(rgb, [DETECT_SIZE, DETECT_SIZE, 3]);
        const detected = await detector.estimateFaces(tensor, {
          flipHorizontal: false,
        });
        tensor.dispose();

        const next = new Float32Array(MAX_FACES * 4);
        const nf = Math.min(detected.length, MAX_FACES);
        for (let i = 0; i < nf; i++) {
          const b = detected[i].box;
          next[i * 4 + 0] = b.xMin / DETECT_SIZE;
          next[i * 4 + 1] = b.yMin / DETECT_SIZE;
          next[i * 4 + 2] = b.width / DETECT_SIZE;
          next[i * 4 + 3] = b.height / DETECT_SIZE;
        }
        faces = next;
        numFaces = nf;
      } catch (e) {
        console.warn("[FaceDetection] detection failed", e);
      } finally {
        detectionBusy = false;
        if (!disposed) {
          // Yield to the rAF loop, then queue up the next inference. The
          // detector + readback together set the effective detection rate.
          setTimeout(runDetection, 0);
        }
      }
    };

    const detectorStartTimer = setInterval(() => {
      if (detector && !detectionBusy) {
        clearInterval(detectorStartTimer);
        runDetection();
      }
    }, 100);

    const startTime = performance.now();
    const render = () => {
      const newFrame = player.copyLatestFrame();
      if (newFrame) {
        if (currentFrame) {
          currentFrame.release();
        }
        currentFrame = newFrame;
      }

      const encoder = device.createCommandEncoder();
      let externalTex: GPUExternalTexture | null = null;
      if (currentFrame) {
        try {
          externalTex = device.importExternalTexture({
            source: currentFrame,
            label: "video-external",
          });
        } catch (e) {
          console.warn("[FaceDetection] importExternalTexture failed:", e);
        }
      }

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

      if (externalTex && currentFrame) {
        uniformF32[0] = currentFrame.width;
        uniformF32[1] = currentFrame.height;
        uniformF32[2] = canvas.width;
        uniformF32[3] = canvas.height;
        uniformU32[4] = numFaces;
        uniformF32[5] = (performance.now() - startTime) / 1000;
        uniformF32[6] = 0;
        uniformF32[7] = 0;
        // The faces array starts at byte 32 (index 8 in the F32 view).
        uniformF32.set(faces, 8);
        device.queue.writeBuffer(uniformBuffer, 0, uniformData);

        const bindGroup = device.createBindGroup({
          layout: pipeline.getBindGroupLayout(0),
          entries: [
            { binding: 0, resource: externalTex },
            { binding: 1, resource: sampler },
            { binding: 2, resource: { buffer: uniformBuffer } },
          ],
        });
        pass.setPipeline(pipeline);
        pass.setBindGroup(0, bindGroup);
        pass.draw(3);
      }

      pass.end();
      device.queue.submit([encoder.finish()]);
      context.present();
      rafRef.current = requestAnimationFrame(render);
    };
    rafRef.current = requestAnimationFrame(render);

    return () => {
      disposed = true;
      clearInterval(detectorStartTimer);
      if (rafRef.current !== null) {
        cancelAnimationFrame(rafRef.current);
      }
      if (currentFrame) {
        currentFrame.release();
        currentFrame = null;
      }
      uniformBuffer.destroy();
      detectTex.destroy();
      detectReadBuffer.destroy();
      player.release();
    };
  }, [device, adapter, ref]);

  if (error) {
    return (
      <View style={styles.errorContainer}>
        <Text style={styles.errorText}>{error}</Text>
      </View>
    );
  }
  return (
    <View style={styles.root}>
      <Canvas ref={ref} style={styles.canvas} />
      <View style={styles.statusBar}>
        <Text style={styles.statusText}>{status}</Text>
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  root: { flex: 1, backgroundColor: "black" },
  canvas: { flex: 1 },
  statusBar: {
    position: "absolute",
    top: 16,
    left: 16,
    backgroundColor: "rgba(0,0,0,0.55)",
    paddingHorizontal: 10,
    paddingVertical: 6,
    borderRadius: 6,
  },
  statusText: { color: "white", fontSize: 12 },
  errorContainer: { flex: 1, padding: 16, justifyContent: "center" },
  errorText: { color: "red", fontSize: 14 },
});
