import React, { useEffect, useRef, useState } from "react";
import { PixelRatio, Platform, StyleSheet, Text, View } from "react-native";
import {
  Canvas,
  useCanvasRef,
  useDevice,
  type NativeCanvas,
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

const REQUIRED_FEATURE =
  Platform.OS === "ios"
    ? "shared-texture-memory-iosurface"
    : "shared-texture-memory-ahardware-buffer";

export const SharedTextureMemory = () => {
  const ref = useCanvasRef();
  const [error, setError] = useState<string | null>(null);
  const rafRef = useRef<number | null>(null);

  // Request the shared-memory feature when constructing the device so the
  // shared-texture-memory* extension is enabled.
  const { device, adapter } = useDevice(undefined, {
    // Cast: GPUFeatureName in @webgpu/types doesn't include the Dawn-specific
    // extension name yet, but Dawn accepts it.
    requiredFeatures: [REQUIRED_FEATURE as GPUFeatureName],
  });

  useEffect(() => {
    if (!device) {
      return;
    }
    if (!device.features.has(REQUIRED_FEATURE)) {
      setError(
        `Device is missing the '${REQUIRED_FEATURE}' feature (adapter supports: ${
          adapter
            ? [...adapter.features]
                .filter((f) => f.toString().startsWith("shared-"))
                .join(", ") || "none"
            : "n/a"
        })`,
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

    // 1. Acquire a native, GPU-shareable surface. In production this would
    //    come from a camera frame processor or video decoder. The test helper
    //    synthesizes a 256x256 RGB-gradient pattern in an IOSurface.
    const frame = RNWebGPU.createTestVideoFrame(256, 256);

    // 2. Import the raw native handle into a SharedTextureMemory.
    const sharedMemory = device.importSharedTextureMemory({
      handle: frame.handle,
      label: "video-frame-shared-memory",
    });

    // 3. Create a regular GPUTexture that aliases the surface's pixels.
    //    No descriptor needed: the format/size are inferred from the surface.
    const texture = sharedMemory.createTexture();

    // 4. beginAccess declares that we're about to read or write the texture on
    //    the GPU timeline. `initialized: true` means "the surface already has
    //    meaningful pixels", which is correct for an incoming video frame.
    //
    //    Because this example owns a *static* IOSurface (no external producer
    //    is writing new pixels between frames), we keep one access window open
    //    for the lifetime of the texture and call endAccess only on unmount.
    //
    //    For a live camera or video feed, you'd instead wrap each frame:
    //      beginAccess(tex, true) -> submit -> endAccess(tex)
    //    around every render to hand ownership back to the producer. That's
    //    also where fence support (not yet wired through this binding) becomes
    //    important to avoid races with the producer.
    if (!sharedMemory.beginAccess(texture, true)) {
      setError("beginAccess() failed");
      return;
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
    const bindGroup = device.createBindGroup({
      layout: pipeline.getBindGroupLayout(0),
      entries: [
        { binding: 0, resource: texture.createView() },
        { binding: 1, resource: sampler },
      ],
    });

    const render = () => {
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
      pass.setPipeline(pipeline);
      pass.setBindGroup(0, bindGroup);
      pass.draw(3);
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
      sharedMemory.endAccess(texture);
      texture.destroy();
      frame.release();
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
