import React, { useEffect, useRef, useState } from "react";
import { PixelRatio, StyleSheet, Text, View } from "react-native";
import {
  Canvas,
  useCanvasRef,
  useDevice,
  type NativeCanvas,
  type VideoFrame,
} from "react-native-wgpu";

import { EffectToolbar } from "./EffectToolbar";
import { INITIAL_MODES, type Modes } from "./features";
import { SHADER } from "./shader";

// rnwebgpu/shared-texture-memory is our umbrella that expands to the
// platform's shared-memory + shared-fence pair (the IOSurface / AHB still
// flows through SharedTextureMemory under the hood). Plus
// dawn-multi-planar-formats so Dawn can interpret the NV12 surface as a
// biplanar texture.
const REQUIRED_FEATURES: GPUFeatureName[] = [
  "rnwebgpu/shared-texture-memory" as GPUFeatureName,
  "dawn-multi-planar-formats" as GPUFeatureName,
];

const VIDEO_URL =
  "https://test-videos.co.uk/vids/bigbuckbunny/mp4/h264/1080/Big_Buck_Bunny_1080_10s_5MB.mp4";

export const ExternalTexture = () => {
  const ref = useCanvasRef();
  const [error, setError] = useState<string | null>(null);
  const rafRef = useRef<number | null>(null);

  const { device, adapter } = useDevice(undefined, {
    requiredFeatures: REQUIRED_FEATURES,
  });

  // Refs let the render loop pick up new mode values without re-running the
  // useEffect (which would tear down and rebuild the pipeline + player).
  const modesRef = useRef<Modes>(INITIAL_MODES);
  const [modes, setModes] = useState<Modes>(INITIAL_MODES);
  const cycle = (key: keyof Modes, max: number) => {
    const next = {
      ...modesRef.current,
      [key]: (modesRef.current[key] + 1) % max,
    };
    modesRef.current = next;
    setModes(next);
  };

  useEffect(() => {
    if (!device) {
      return;
    }
    const missing = REQUIRED_FEATURES.filter((f) => !device.features.has(f));
    if (missing.length > 0) {
      setError(
        `Device is missing required features [${missing.join(", ")}]. Adapter supports: ${
          adapter
            ? [...adapter.features]
                .filter((f) => f.toString().startsWith("shared-"))
                .join(", ") || "none"
            : "n/a"
        }`,
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

    // Same Big Buck Bunny URL as the SharedTextureMemory demo, but ask AVPlayer
    // for native NV12 instead of BGRA. Each VideoFrame now carries the YUV
    // matrix + plane info that importExternalTexture needs.
    const player = RNWebGPU.createVideoPlayer(VIDEO_URL, "nv12");
    player.play();

    const module = device.createShaderModule({ code: SHADER });
    const pipeline = device.createRenderPipeline({
      layout: "auto",
      vertex: { module, entryPoint: "vs_main" },
      fragment: {
        module,
        entryPoint: "fs_main",
        targets: [{ format: presentationFormat }],
      },
      primitive: { topology: "triangle-list" },
    });
    const sampler = device.createSampler({
      magFilter: "linear",
      minFilter: "linear",
    });

    // 48-byte uniform: vec2 texSize, vec2 canvasSize, 5 u32 mode flags,
    // f32 time, vec2 padding. Built on a single ArrayBuffer so we can pack
    // mixed f32/u32 fields in one writeBuffer call per frame.
    const uniformBuffer = device.createBuffer({
      size: 48,
      usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
    });
    const uniformData = new ArrayBuffer(48);
    const uniformF32 = new Float32Array(uniformData);
    const uniformU32 = new Uint32Array(uniformData);

    // The video plays at ~24fps but we tick at the display's 60Hz, so most rAF
    // ticks have no new frame from AVPlayer. Hold the latest VideoFrame across
    // ticks and re-import an ExternalTexture from it on the "no new frame"
    // ticks, this is what stops the canvas from flashing black ~2/3 of the
    // time. AVPlayer's pool is several buffers deep so holding one back like
    // this doesn't stall decoding.
    let currentFrame: VideoFrame | null = null;

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

      if (currentFrame) {
        // GPUExternalTexture expires after each submit, so we rebuild one
        // every tick, even when sampling the same VideoFrame as last tick.
        let externalTex: GPUExternalTexture | null = null;
        try {
          externalTex = device.importExternalTexture({
            source: currentFrame,
            label: "video-external",
          });
        } catch (e) {
          console.warn("[ExternalTexture] importExternalTexture failed:", e);
        }

        if (externalTex) {
          const m = modesRef.current;
          uniformF32[0] = currentFrame.width;
          uniformF32[1] = currentFrame.height;
          uniformF32[2] = canvas.width;
          uniformF32[3] = canvas.height;
          uniformU32[4] = m.resize;
          uniformU32[5] = m.effect;
          uniformU32[6] = m.color;
          uniformU32[7] = m.ambient;
          uniformU32[8] = m.glass;
          uniformF32[9] = (performance.now() - startTime) / 1000;
          uniformF32[10] = 0;
          uniformF32[11] = 0;
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
      }

      pass.end();
      device.queue.submit([encoder.finish()]);
      context.present();
      rafRef.current = requestAnimationFrame(render);
    };
    rafRef.current = requestAnimationFrame(render);

    return () => {
      if (rafRef.current !== null) {
        cancelAnimationFrame(rafRef.current);
      }
      if (currentFrame) {
        currentFrame.release();
        currentFrame = null;
      }
      uniformBuffer.destroy();
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
      <EffectToolbar modes={modes} onCycle={cycle} />
    </View>
  );
};

const styles = StyleSheet.create({
  root: { flex: 1, backgroundColor: "black" },
  canvas: { flex: 1 },
  errorContainer: { flex: 1, padding: 16, justifyContent: "center" },
  errorText: { color: "red", fontSize: 14 },
});
