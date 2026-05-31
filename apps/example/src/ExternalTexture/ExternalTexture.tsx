import React, { useEffect, useRef, useState } from "react";
import { PixelRatio, StyleSheet, Text, View } from "react-native";
import {
  Canvas,
  useCanvasRef,
  useDevice,
  type NativeCanvas,
  type NativeVideoFrame,
} from "react-native-wgpu";

// importExternalTexture is the spec-mandated path for "I have a YUV-encoded
// video/camera frame and I want to sample it in a shader without copying or
// hand-rolling YUV math". The WGSL side uses texture_external +
// textureSampleBaseClampToEdge; the driver does the planar fetch, YUV→RGB
// matrix multiply, sRGB transfer, and gamut conversion in the sampler.
//
// Bind groups for texture_external use auto layout slots like any other
// resource. WGSL doesn't expose the underlying plane textures directly.
const SHADER = /* wgsl */ `
struct VsOut {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
};

struct Uniforms {
  uvScale: vec2f,
};

@group(0) @binding(0) var srcTex: texture_external;
@group(0) @binding(1) var srcSampler: sampler;
@group(0) @binding(2) var<uniform> u: Uniforms;

@vertex
fn vs_main(@builtin(vertex_index) vid: u32) -> VsOut {
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

    // One persistent uniform buffer; we rewrite its uvScale whenever a new
    // frame's dimensions differ from the last one.
    const uniformBuffer = device.createBuffer({
      size: 16,
      usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
    });
    let lastUvScale: [number, number] | null = null;
    const writeUvScale = (texW: number, texH: number) => {
      const canvasAR = canvas.width / canvas.height;
      const texAR = texW / texH;
      const next: [number, number] =
        texAR > canvasAR ? [canvasAR / texAR, 1] : [1, texAR / canvasAR];
      if (
        !lastUvScale ||
        lastUvScale[0] !== next[0] ||
        lastUvScale[1] !== next[1]
      ) {
        device.queue.writeBuffer(uniformBuffer, 0, new Float32Array(next));
        lastUvScale = next;
      }
    };

    // The video plays at ~24fps but we tick at the display's 60Hz, so most rAF
    // ticks have no new frame from AVPlayer. Hold the latest VideoFrame across
    // ticks and re-import an ExternalTexture from it on the "no new frame"
    // ticks — this is what stops the canvas from flashing black ~2/3 of the
    // time. AVPlayer's pool is several buffers deep so holding one back like
    // this doesn't stall decoding.
    let currentFrame: NativeVideoFrame | null = null;

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
        // every tick — even when sampling the same VideoFrame as last tick.
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
          writeUvScale(currentFrame.width, currentFrame.height);
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
    <View style={{ flex: 1 }}>
      <Canvas ref={ref} style={{ flex: 1 }} />
    </View>
  );
};

const styles = StyleSheet.create({
  errorContainer: { flex: 1, padding: 16, justifyContent: "center" },
  errorText: { color: "red", fontSize: 14 },
});
