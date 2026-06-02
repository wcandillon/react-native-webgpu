import React, { useEffect, useRef, useState } from "react";
import { PixelRatio, Platform, StyleSheet, Text, View } from "react-native";
import {
  Canvas,
  useCanvasRef,
  useDevice,
  type NativeCanvas,
  type NativeVideoFrame,
  type VideoPlayer,
} from "react-native-wgpu";

// This is the SharedTextureMemory demo, rewritten to use
// GPUDevice.importExternalTexture instead of the manual
// importSharedTextureMemory + createTexture + beginAccess/endAccess dance.
//
// The visible result is identical (the same video frame stretched 'cover' over
// the canvas), but the sampling path differs: the source is bound as a
// `texture_external` and read with textureSampleBaseClampToEdge, and Dawn owns
// the shared-memory begin/end-access lifecycle internally. A GPUExternalTexture
// expires once the queue work that used it is submitted, so we re-import one
// every frame.
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

@group(0) @binding(0) var srcTex: texture_external;
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
  return textureSampleBaseClampToEdge(srcTex, srcSampler, uv);
}
`;

// importExternalTexture is backed by the "rnwebgpu/native-texture" umbrella
// feature (it imports the frame's IOSurface / AHardwareBuffer as shared texture
// memory, then wraps that as an external texture). Like importExternalTexture on
// the web, that capability is now enabled by default: requestDevice / useDevice
// turns it on automatically whenever the adapter supports it, so we don't pass
// anything in requiredFeatures. We keep the name only to feature-detect and
// degrade gracefully on hardware that doesn't support it.
const FEATURE = "rnwebgpu/native-texture" as GPUFeatureName;

export const ImportExternalTexture = () => {
  const ref = useCanvasRef();
  const [error, setError] = useState<string | null>(null);
  const rafRef = useRef<number | null>(null);

  const { device, adapter } = useDevice();

  useEffect(() => {
    if (!device) {
      return;
    }
    // native-texture is auto-enabled when supported; if it is still missing,
    // this hardware/driver can't back importExternalTexture.
    if (!device.features.has(FEATURE)) {
      setError(
        `This device doesn't support ${FEATURE} (importExternalTexture). Adapter supports: ${
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

    // Pick a frame source per platform. On iOS we stream a real video via
    // AVPlayer; elsewhere we don't have a video pipeline yet, so we use a
    // single synthetic IOSurface/AHardwareBuffer frame. A VideoPlayer exposes
    // copyLatestFrame() (a fresh frame each tick) while a NativeVideoFrame does
    // not — the render loop tells them apart with that property.
    let source: VideoPlayer | NativeVideoFrame;
    if (Platform.OS === "ios") {
      const VIDEO_URL =
        "https://test-videos.co.uk/vids/bigbuckbunny/mp4/h264/1080/Big_Buck_Bunny_1080_10s_5MB.mp4";
      const player = RNWebGPU.createVideoPlayer(VIDEO_URL);
      player.play();
      source = player;
    } else {
      source = RNWebGPU.createTestVideoFrame(1024, 1024);
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
    // One persistent uniform buffer; rewritten whenever the frame dimensions
    // change.
    const uniformBuffer = device.createBuffer({
      size: 16, // vec2<f32> padded to 16-byte uniform alignment
      usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
    });

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

    // Hold the current frame across rAF ticks. For a VideoPlayer we pull the
    // latest frame each tick (keeping the previous one when the decoder hasn't
    // produced a new one yet, to avoid a black flash); for a one-shot
    // NativeVideoFrame we just keep re-importing the same frame.
    let currentFrame: NativeVideoFrame | null =
      "copyLatestFrame" in source ? null : source;
    let lastDims: [number, number] | null = null;

    const render = () => {
      if ("copyLatestFrame" in source) {
        const newFrame = source.copyLatestFrame();
        if (newFrame) {
          if (currentFrame) {
            currentFrame.release();
          }
          currentFrame = newFrame;
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

      // A GPUExternalTexture expires after each submit, so re-import one every
      // tick, even when sampling the same frame as last tick. Unlike on the
      // web, the shared-memory begin/end-access window is tied to this
      // wrapper's lifetime, so we destroy() it right after submit (below) to
      // release the surface promptly instead of waiting for GC.
      let externalTex: GPUExternalTexture | null = null;
      if (currentFrame) {
        try {
          externalTex = device.importExternalTexture({
            source: currentFrame,
            label: "video-frame",
          });
        } catch (e) {
          console.warn("[ImportExternalTexture] import failed:", e);
        }

        if (externalTex) {
          if (
            !lastDims ||
            lastDims[0] !== currentFrame.width ||
            lastDims[1] !== currentFrame.height
          ) {
            const [sx, sy] = computeUvScale(
              currentFrame.width,
              currentFrame.height,
            );
            device.queue.writeBuffer(
              uniformBuffer,
              0,
              new Float32Array([sx, sy]),
            );
            lastDims = [currentFrame.width, currentFrame.height];
          }
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
      // Now that the work sampling it has been submitted, end the external
      // texture's access window so the frame's surface is released promptly.
      externalTex?.destroy();
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
      // For the player, release it; the one-shot frame was released above as
      // currentFrame (same object), so don't double-release it here.
      if ("copyLatestFrame" in source) {
        source.release();
      }
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
