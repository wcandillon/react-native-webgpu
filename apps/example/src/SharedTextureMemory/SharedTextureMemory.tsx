import React, { useEffect, useRef, useState } from "react";
import { PixelRatio, Platform, StyleSheet, Text, View } from "react-native";
import {
  Canvas,
  useCanvasRef,
  useDevice,
  type GPUSharedTextureMemory,
  type NativeCanvas,
  type VideoFrame,
} from "react-native-wgpu";

const SHADER = /* wgsl */ `
struct VsOut {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
};

struct Uniforms {
  // Per-axis scale applied to UVs *around the center* so that the canvas
  // samples a sub-rectangle of the texture matching the canvas aspect ratio.
  // 'cover' fit: one axis is 1.0, the other is canvasAR / textureAR (or its
  // reciprocal), whichever is < 1 — i.e. we crop on the longer axis.
  uvScale: vec2f,
};

@group(0) @binding(0) var srcTex: texture_2d<f32>;
@group(0) @binding(1) var srcSampler: sampler;
@group(0) @binding(2) var<uniform> u: Uniforms;

@vertex
fn vs_main(@builtin(vertex_index) vid: u32) -> VsOut {
  // Full-screen triangle.
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

@fragment
fn fs_main(in: VsOut) -> @location(0) vec4f {
  let uv = vec2f(0.5) + (in.uv - vec2f(0.5)) * u.uvScale;
  return textureSample(srcTex, srcSampler, uv);
}
`;

const REQUIRED_FEATURES = ["rnwebgpu/shared-texture-memory" as GPUFeatureName];

export const SharedTextureMemory = () => {
  const ref = useCanvasRef();
  const [error, setError] = useState<string | null>(null);
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

    // Pick a frame source per platform. On iOS we use AVPlayer to stream a
    // real video; on Android we don't have a video pipeline yet, so we fall
    // back to a single synthetic IOSurface/AHardwareBuffer frame produced by
    // RNWebGPU.createTestVideoFrame. The rAF loop below treats a null return
    // from copyLatestFrame() as "keep showing the previous frame", which means
    // a one-shot source renders correctly without any other change.
    interface FrameSource {
      copyLatestFrame(): VideoFrame | null;
      release(): void;
    }
    let source: FrameSource;
    if (Platform.OS === "ios") {
      const VIDEO_URL =
        "https://test-videos.co.uk/vids/bigbuckbunny/mp4/h264/1080/Big_Buck_Bunny_1080_10s_5MB.mp4";
      const player = RNWebGPU.createVideoPlayer(VIDEO_URL);
      player.play();
      source = {
        copyLatestFrame: () => player.copyLatestFrame(),
        release: () => player.release(),
      };
    } else {
      let pending: VideoFrame | null = RNWebGPU.createTestVideoFrame(
        1024,
        1024,
      );
      source = {
        copyLatestFrame: () => {
          const f = pending;
          pending = null;
          return f;
        },
        release: () => {
          if (pending) {
            pending.release();
            pending = null;
          }
        },
      };
    }

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

    // We hold the *current* frame across rAF ticks so that when the video
    // hasn't produced a new frame yet (between decoder timestamps), we keep
    // rendering the last one rather than dropping to a black screen.
    //
    // For each new IOSurface we:
    //   - create a SharedTextureMemory + texture + bindGroup
    //   - beginAccess(initialized: true) to declare "the producer has written
    //     these pixels and we're now sampling them"
    //   - sample in the shader
    //   - endAccess to hand ownership back to the producer
    //
    // We close out the previous frame's access window first. In a fence-aware
    // build we'd plumb an AVPlayer fence through beginAccess/endAccess; for the
    // demo we rely on AVPlayer recycling its IOSurface pool, which is safe as
    // long as we end-access before letting the player reclaim the buffer.
    type Bound = {
      frame: VideoFrame;
      memory: GPUSharedTextureMemory;
      texture: GPUTexture;
      bindGroup: GPUBindGroup;
      uniformBuffer: GPUBuffer;
    };
    let current: Bound | null = null;

    // 'cover' fit: scale UVs around their center so the longer axis of the
    // texture is cropped to match the canvas aspect ratio.
    const computeUvScale = (texW: number, texH: number): [number, number] => {
      const canvasAR = canvas.width / canvas.height;
      const texAR = texW / texH;
      if (texAR > canvasAR) {
        // Texture is wider than the canvas: crop horizontally.
        return [canvasAR / texAR, 1];
      } else {
        // Texture is taller than (or equal to) the canvas: crop vertically.
        return [1, texAR / canvasAR];
      }
    };

    const bindFrame = (frame: VideoFrame): Bound | null => {
      try {
        const memory = device.importSharedTextureMemory({
          handle: frame.handle,
          label: "video-frame",
        });
        const texture = memory.createTexture();
        memory.beginAccess(texture, true);
        const uniformBuffer = device.createBuffer({
          size: 16, // vec2<f32> padded to 16-byte uniform alignment
          usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
        });
        const [sx, sy] = computeUvScale(frame.width, frame.height);
        device.queue.writeBuffer(uniformBuffer, 0, new Float32Array([sx, sy]));
        const bindGroup = device.createBindGroup({
          layout: pipeline.getBindGroupLayout(0),
          entries: [
            { binding: 0, resource: texture.createView() },
            { binding: 1, resource: sampler },
            { binding: 2, resource: { buffer: uniformBuffer } },
          ],
        });
        return { frame, memory, texture, bindGroup, uniformBuffer };
      } catch (e) {
        console.warn("[SharedTextureMemory] bindFrame failed:", e);
        frame.release();
        return null;
      }
    };

    const releaseBound = (b: Bound) => {
      try {
        b.memory.endAccess(b.texture);
      } catch (e) {
        console.warn("[SharedTextureMemory] endAccess failed:", e);
      }
      b.texture.destroy();
      b.uniformBuffer.destroy();
      b.frame.release();
    };

    const render = () => {
      // Pull the latest frame from the player. Null means "no new frame since
      // we last asked", in which case we keep using the existing one.
      const newFrame = source.copyLatestFrame();
      if (newFrame) {
        const next = bindFrame(newFrame);
        if (next) {
          if (current) {
            releaseBound(current);
          }
          current = next;
        }
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
      if (current) {
        pass.setPipeline(pipeline);
        pass.setBindGroup(0, current.bindGroup);
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
      if (current) {
        releaseBound(current);
        current = null;
      }
      source.release();
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
    <View style={{ flex: 1 }}>
      <Canvas ref={ref} style={{ flex: 1 }} />
    </View>
  );
};

const styles = StyleSheet.create({
  errorContainer: { flex: 1, padding: 16, justifyContent: "center" },
  errorText: { color: "red", fontSize: 14 },
});
