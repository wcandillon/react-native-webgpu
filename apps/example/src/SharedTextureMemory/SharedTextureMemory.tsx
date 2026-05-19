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

@group(0) @binding(0) var srcTex: texture_2d<f32>;
@group(0) @binding(1) var srcSampler: sampler;

@fragment
fn fs_main(in: VsOut) -> @location(0) vec4f {
  return textureSample(srcTex, srcSampler, in.uv);
}
`;

// On Metal, EndAccess on an IOSurface-backed SharedTextureMemory always
// produces an MTLSharedEvent fence (so the producer can wait on the GPU). Even
// though we don't currently expose the fence to JS, Dawn validates that the
// fence feature is enabled before letting EndAccess succeed. Android has the
// equivalent pairing with sync fds.
const REQUIRED_FEATURES =
  Platform.OS === "ios"
    ? ["shared-texture-memory-iosurface", "shared-fence-mtl-shared-event"]
    : [
        "shared-texture-memory-ahardware-buffer",
        "shared-fence-vk-semaphore-sync-fd",
      ];

export const SharedTextureMemory = () => {
  const ref = useCanvasRef();
  const [error, setError] = useState<string | null>(null);
  const rafRef = useRef<number | null>(null);

  const { device, adapter } = useDevice(undefined, {
    // Cast: GPUFeatureName in @webgpu/types doesn't include the Dawn-specific
    // extension names yet, but Dawn accepts them.
    requiredFeatures: REQUIRED_FEATURES as unknown as GPUFeatureName[],
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

    // 1. Open the video and start playback. AVPlayer accepts local file paths
    //    as well as http(s):// URLs and keeps the IOSurface pool up to date
    //    in the background. For a fully offline demo, swap this URL for
    //    RNWebGPU.writeTestVideoFile() which generates a tiny mp4 on disk.
    const VIDEO_URL =
      "https://test-videos.co.uk/vids/bigbuckbunny/mp4/h264/1080/Big_Buck_Bunny_1080_10s_5MB.mp4";
    const player = RNWebGPU.createVideoPlayer(VIDEO_URL);
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
    };
    let current: Bound | null = null;

    const bindFrame = (frame: VideoFrame): Bound | null => {
      try {
        const memory = device.importSharedTextureMemory({
          handle: frame.handle,
          label: "video-frame",
        });
        const texture = memory.createTexture();
        if (!memory.beginAccess(texture, true)) {
          texture.destroy();
          frame.release();
          return null;
        }
        const bindGroup = device.createBindGroup({
          layout: pipeline.getBindGroupLayout(0),
          entries: [
            { binding: 0, resource: texture.createView() },
            { binding: 1, resource: sampler },
          ],
        });
        return { frame, memory, texture, bindGroup };
      } catch (e) {
        console.warn("[SharedTextureMemory] bindFrame failed:", e);
        frame.release();
        return null;
      }
    };

    const releaseBound = (b: Bound) => {
      b.memory.endAccess(b.texture);
      b.texture.destroy();
      b.frame.release();
    };

    const render = () => {
      // Pull the latest frame from the player. Null means "no new frame since
      // we last asked", in which case we keep using the existing one.
      const newFrame = player.copyLatestFrame();
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
    <View style={{ flex: 1 }}>
      <Canvas ref={ref} style={{ flex: 1 }} />
    </View>
  );
};

const styles = StyleSheet.create({
  errorContainer: { flex: 1, padding: 16, justifyContent: "center" },
  errorText: { color: "red", fontSize: 14 },
});
