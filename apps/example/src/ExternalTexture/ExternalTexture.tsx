import React, { useEffect, useRef, useState } from "react";
import { Image, PixelRatio, StyleSheet, Text, View } from "react-native";
import {
  Canvas,
  useCanvasRef,
  useDevice,
  type NativeCanvas,
  type VideoFrame,
} from "react-native-wgpu";

import { BLUR_SHADER, DOWNSAMPLE_SHADER, PREPASS_SHADER } from "./blurShaders";
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

const VIDEO_URL = Image.resolveAssetSource(
  require("../assets/trailer.mp4"),
).uri;

// Ambient-blur tuning. Filter size must be odd; blockDim = TILE_DIM - filterSize
// is how many output rows of useful work each tile produces. Iterating
// box-blurs approximates a gaussian (variance adds), so the perceived sigma
// grows as filterSize * sqrt(iterations).
//
// Two chains:
//   * "Blur" mode renders the prepass at 1/4 res and blurs there.
//   * "On" mode reuses the same prepass, then downsamples 1/4 -> 1/16 with a
//     bilinear blit and blurs the tiny texture. Sigma in screen pixels is
//     sigma-low-res * scale, so each iteration at 1/16 covers 4x more screen
//     than at 1/4. Net: heavier blur for *less* GPU work than piling
//     iterations onto the 1/4 chain would cost.
const BLUR_SCALE = 4;
const SMALL_BLUR_SCALE = 16;
const BLUR_FILTER_SIZE = 31;
const BLUR_TILE_DIM = 128;
const BLUR_BATCH = 4;
const BLUR_BLOCK_DIM = BLUR_TILE_DIM - BLUR_FILTER_SIZE;
const BLUR_ITERATIONS = 2;
const SMALL_BLUR_ITERATIONS = 4;

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

    // ----- Ambient-blur infrastructure -----------------------------------
    const blurWidth = Math.max(
      BLUR_TILE_DIM,
      Math.ceil(canvas.width / BLUR_SCALE),
    );
    const blurHeight = Math.max(
      BLUR_TILE_DIM,
      Math.ceil(canvas.height / BLUR_SCALE),
    );

    // Prepass: external (YUV) -> rgba8unorm at 1/4 canvas, cover-projected.
    const prepassModule = device.createShaderModule({ code: PREPASS_SHADER });
    const prepassPipeline = device.createRenderPipeline({
      layout: "auto",
      vertex: { module: prepassModule, entryPoint: "vs_main" },
      fragment: {
        module: prepassModule,
        entryPoint: "fs_main",
        targets: [{ format: "rgba8unorm" }],
      },
      primitive: { topology: "triangle-list" },
    });
    const prepassUniformBuffer = device.createBuffer({
      size: 16,
      usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
    });
    const prepassUniformData = new Float32Array(4);

    // Target of the prepass; input of the first blur tap.
    const blurSrcTexture = device.createTexture({
      size: [blurWidth, blurHeight],
      format: "rgba8unorm",
      usage:
        GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.TEXTURE_BINDING,
    });
    // Ping-pong pair for the separable blur. Final result lives in blurPing[1].
    const blurPing = [0, 1].map(() =>
      device.createTexture({
        size: [blurWidth, blurHeight],
        format: "rgba8unorm",
        usage:
          GPUTextureUsage.STORAGE_BINDING | GPUTextureUsage.TEXTURE_BINDING,
      }),
    );

    // Compute pipeline. The WebGPU-samples tile blur, used unmodified.
    const blurPipeline = device.createComputePipeline({
      layout: "auto",
      compute: {
        module: device.createShaderModule({ code: BLUR_SHADER }),
        entryPoint: "main",
      },
    });

    // flip=0 means "blur along x", flip=1 means "blur along y" (the shader
    // swaps loadIndex.xy at sample/store time). Two static uniform buffers
    // are simpler than rewriting a single one per dispatch.
    const flip0Buffer = device.createBuffer({
      size: 4,
      mappedAtCreation: true,
      usage: GPUBufferUsage.UNIFORM,
    });
    new Uint32Array(flip0Buffer.getMappedRange())[0] = 0;
    flip0Buffer.unmap();
    const flip1Buffer = device.createBuffer({
      size: 4,
      mappedAtCreation: true,
      usage: GPUBufferUsage.UNIFORM,
    });
    new Uint32Array(flip1Buffer.getMappedRange())[0] = 1;
    flip1Buffer.unmap();

    // Reference uses filterSize+1 here, then divides offsets by it; see the
    // BLUR_SHADER comment in blurShaders.ts. Block dim = useful output rows
    // per tile (we ceil-dispatch off that, the shader bounds-checks the rest).
    const blurParamsBuffer = device.createBuffer({
      size: 8,
      usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
    });
    device.queue.writeBuffer(
      blurParamsBuffer,
      0,
      new Uint32Array([BLUR_FILTER_SIZE + 1, BLUR_BLOCK_DIM]),
    );

    const blurConstants = device.createBindGroup({
      layout: blurPipeline.getBindGroupLayout(0),
      entries: [
        { binding: 0, resource: sampler },
        { binding: 1, resource: { buffer: blurParamsBuffer } },
      ],
    });
    // H: blurSrcTexture -> blurPing[0]
    const blurBindGroup0 = device.createBindGroup({
      layout: blurPipeline.getBindGroupLayout(1),
      entries: [
        { binding: 1, resource: blurSrcTexture.createView() },
        { binding: 2, resource: blurPing[0].createView() },
        { binding: 3, resource: { buffer: flip0Buffer } },
      ],
    });
    // V: blurPing[0] -> blurPing[1]
    const blurBindGroup1 = device.createBindGroup({
      layout: blurPipeline.getBindGroupLayout(1),
      entries: [
        { binding: 1, resource: blurPing[0].createView() },
        { binding: 2, resource: blurPing[1].createView() },
        { binding: 3, resource: { buffer: flip1Buffer } },
      ],
    });
    // H (iteration): blurPing[1] -> blurPing[0]
    const blurBindGroup2 = device.createBindGroup({
      layout: blurPipeline.getBindGroupLayout(1),
      entries: [
        { binding: 1, resource: blurPing[1].createView() },
        { binding: 2, resource: blurPing[0].createView() },
        { binding: 3, resource: { buffer: flip0Buffer } },
      ],
    });
    const blurredView = blurPing[1].createView();

    // ----- "On" mode: downsample 1/4 -> 1/16 then blur the tiny texture --
    const smallWidth = Math.ceil(canvas.width / SMALL_BLUR_SCALE);
    const smallHeight = Math.ceil(canvas.height / SMALL_BLUR_SCALE);

    const smallBlurSrcTexture = device.createTexture({
      size: [smallWidth, smallHeight],
      format: "rgba8unorm",
      usage:
        GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.TEXTURE_BINDING,
    });
    const smallPing = [0, 1].map(() =>
      device.createTexture({
        size: [smallWidth, smallHeight],
        format: "rgba8unorm",
        usage:
          GPUTextureUsage.STORAGE_BINDING | GPUTextureUsage.TEXTURE_BINDING,
      }),
    );

    const downsampleModule = device.createShaderModule({
      code: DOWNSAMPLE_SHADER,
    });
    const downsamplePipeline = device.createRenderPipeline({
      layout: "auto",
      vertex: { module: downsampleModule, entryPoint: "vs_main" },
      fragment: {
        module: downsampleModule,
        entryPoint: "fs_main",
        targets: [{ format: "rgba8unorm" }],
      },
      primitive: { topology: "triangle-list" },
    });
    const downsampleBindGroup = device.createBindGroup({
      layout: downsamplePipeline.getBindGroupLayout(0),
      entries: [
        { binding: 0, resource: blurSrcTexture.createView() },
        { binding: 1, resource: sampler },
      ],
    });

    const smallBlurBindGroup0 = device.createBindGroup({
      layout: blurPipeline.getBindGroupLayout(1),
      entries: [
        { binding: 1, resource: smallBlurSrcTexture.createView() },
        { binding: 2, resource: smallPing[0].createView() },
        { binding: 3, resource: { buffer: flip0Buffer } },
      ],
    });
    const smallBlurBindGroup1 = device.createBindGroup({
      layout: blurPipeline.getBindGroupLayout(1),
      entries: [
        { binding: 1, resource: smallPing[0].createView() },
        { binding: 2, resource: smallPing[1].createView() },
        { binding: 3, resource: { buffer: flip1Buffer } },
      ],
    });
    const smallBlurBindGroup2 = device.createBindGroup({
      layout: blurPipeline.getBindGroupLayout(1),
      entries: [
        { binding: 1, resource: smallPing[1].createView() },
        { binding: 2, resource: smallPing[0].createView() },
        { binding: 3, resource: { buffer: flip0Buffer } },
      ],
    });
    const smallBlurredView = smallPing[1].createView();
    // ----- end ambient-blur infrastructure -------------------------------

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

      // Re-import once per tick; the same external texture feeds both the
      // prepass and the main pass.
      let externalTex: GPUExternalTexture | null = null;
      if (currentFrame) {
        try {
          externalTex = device.importExternalTexture({
            source: currentFrame,
            label: "video-external",
          });
        } catch (e) {
          console.warn("[ExternalTexture] importExternalTexture failed:", e);
        }
      }

      const m = modesRef.current;

      // Prepass is shared by both ambient modes. Skipped when ambient is off;
      // the bound blurred texture keeps its previous contents but the main
      // fragment won't sample it.
      if (m.ambient > 0 && externalTex !== null && currentFrame) {
        prepassUniformData[0] = currentFrame.width;
        prepassUniformData[1] = currentFrame.height;
        prepassUniformData[2] = canvas.width;
        prepassUniformData[3] = canvas.height;
        device.queue.writeBuffer(prepassUniformBuffer, 0, prepassUniformData);

        const prepassBindGroup = device.createBindGroup({
          layout: prepassPipeline.getBindGroupLayout(0),
          entries: [
            { binding: 0, resource: externalTex },
            { binding: 1, resource: sampler },
            { binding: 2, resource: { buffer: prepassUniformBuffer } },
          ],
        });
        const prepass = encoder.beginRenderPass({
          colorAttachments: [
            {
              view: blurSrcTexture.createView(),
              clearValue: { r: 0, g: 0, b: 0, a: 1 },
              loadOp: "clear",
              storeOp: "store",
            },
          ],
        });
        prepass.setPipeline(prepassPipeline);
        prepass.setBindGroup(0, prepassBindGroup);
        prepass.draw(3);
        prepass.end();

        if (m.ambient === 1) {
          // "Blur" mode: separable blur on the 1/4-res prepass output.
          const compute = encoder.beginComputePass();
          compute.setPipeline(blurPipeline);
          compute.setBindGroup(0, blurConstants);
          compute.setBindGroup(1, blurBindGroup0);
          compute.dispatchWorkgroups(
            Math.ceil(blurWidth / BLUR_BLOCK_DIM),
            Math.ceil(blurHeight / BLUR_BATCH),
          );
          compute.setBindGroup(1, blurBindGroup1);
          compute.dispatchWorkgroups(
            Math.ceil(blurHeight / BLUR_BLOCK_DIM),
            Math.ceil(blurWidth / BLUR_BATCH),
          );
          for (let i = 0; i < BLUR_ITERATIONS - 1; i++) {
            compute.setBindGroup(1, blurBindGroup2);
            compute.dispatchWorkgroups(
              Math.ceil(blurWidth / BLUR_BLOCK_DIM),
              Math.ceil(blurHeight / BLUR_BATCH),
            );
            compute.setBindGroup(1, blurBindGroup1);
            compute.dispatchWorkgroups(
              Math.ceil(blurHeight / BLUR_BLOCK_DIM),
              Math.ceil(blurWidth / BLUR_BATCH),
            );
          }
          compute.end();
        } else {
          // "On" mode: skip the 1/4 blur. Downsample 1/4 -> 1/16 and blur
          // the tiny texture; 4x scale buys ~4x more screen-space sigma per
          // iteration, with 16x fewer pixels per pass.
          const downsamplePass = encoder.beginRenderPass({
            colorAttachments: [
              {
                view: smallBlurSrcTexture.createView(),
                clearValue: { r: 0, g: 0, b: 0, a: 1 },
                loadOp: "clear",
                storeOp: "store",
              },
            ],
          });
          downsamplePass.setPipeline(downsamplePipeline);
          downsamplePass.setBindGroup(0, downsampleBindGroup);
          downsamplePass.draw(3);
          downsamplePass.end();

          const compute = encoder.beginComputePass();
          compute.setPipeline(blurPipeline);
          compute.setBindGroup(0, blurConstants);
          compute.setBindGroup(1, smallBlurBindGroup0);
          compute.dispatchWorkgroups(
            Math.ceil(smallWidth / BLUR_BLOCK_DIM),
            Math.ceil(smallHeight / BLUR_BATCH),
          );
          compute.setBindGroup(1, smallBlurBindGroup1);
          compute.dispatchWorkgroups(
            Math.ceil(smallHeight / BLUR_BLOCK_DIM),
            Math.ceil(smallWidth / BLUR_BATCH),
          );
          for (let i = 0; i < SMALL_BLUR_ITERATIONS - 1; i++) {
            compute.setBindGroup(1, smallBlurBindGroup2);
            compute.dispatchWorkgroups(
              Math.ceil(smallWidth / BLUR_BLOCK_DIM),
              Math.ceil(smallHeight / BLUR_BATCH),
            );
            compute.setBindGroup(1, smallBlurBindGroup1);
            compute.dispatchWorkgroups(
              Math.ceil(smallHeight / BLUR_BLOCK_DIM),
              Math.ceil(smallWidth / BLUR_BATCH),
            );
          }
          compute.end();
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
            {
              binding: 3,
              resource: m.ambient === 2 ? smallBlurredView : blurredView,
            },
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
      if (rafRef.current !== null) {
        cancelAnimationFrame(rafRef.current);
      }
      if (currentFrame) {
        currentFrame.release();
        currentFrame = null;
      }
      uniformBuffer.destroy();
      prepassUniformBuffer.destroy();
      flip0Buffer.destroy();
      flip1Buffer.destroy();
      blurParamsBuffer.destroy();
      blurSrcTexture.destroy();
      blurPing[0].destroy();
      blurPing[1].destroy();
      smallBlurSrcTexture.destroy();
      smallPing[0].destroy();
      smallPing[1].destroy();
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
