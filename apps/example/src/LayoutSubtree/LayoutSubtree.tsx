import React, { useEffect, useRef, useState } from "react";
import {
  findNodeHandle,
  PixelRatio,
  StyleSheet,
  Text,
  View,
} from "react-native";
import type { NativeCanvas } from "react-native-webgpu";
import { Canvas, useCanvasRef, useDevice } from "react-native-webgpu";

// Samples the captured element texture onto a full-screen triangle. Sampling
// (rather than a raw copy into the canvas) sidesteps the rgba8/bgra8 format
// mismatch between the AHardwareBuffer and the swapchain.
const shader = /* wgsl */ `
struct VSOut {
  @builtin(position) pos: vec4f,
  @location(0) uv: vec2f,
};

@vertex
fn vmain(@builtin(vertex_index) i: u32) -> VSOut {
  var p = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
  var out: VSOut;
  out.pos = vec4f(p[i], 0.0, 1.0);
  out.uv = p[i] * vec2f(0.5, -0.5) + vec2f(0.5, 0.5);
  return out;
}

@group(0) @binding(0) var samp: sampler;
@group(0) @binding(1) var tex: texture_2d<f32>;

@fragment
fn fmain(in: VSOut) -> @location(0) vec4f {
  return textureSample(tex, samp, in.uv);
}`;

// "HTML in Canvas" demo: a native <View> child of a <Canvas layoutSubtree> is
// rendered off-screen and painted into the WebGPU scene via
// queue.copyElementImageToTexture. Android-only for now (API 34+).
export function LayoutSubtree() {
  const ref = useCanvasRef();
  const contentRef = useRef<View>(null);
  // The captured element's pixel size, measured from its own layout (which can
  // differ from the canvas size). We size the destination texture to it so the
  // sampled result keeps the view's aspect ratio.
  const contentSizeRef = useRef<{ w: number; h: number } | null>(null);
  const [error, setError] = useState<string | null>(null);
  const { device } = useDevice();

  useEffect(() => {
    if (!device) {
      return;
    }
    let raf = 0;
    let cancelled = false;

    const context = ref.current?.getContext("webgpu");
    if (!context) {
      return;
    }
    const canvas = context.canvas as unknown as NativeCanvas;
    const width = Math.floor(canvas.clientWidth * PixelRatio.get());
    const height = Math.floor(canvas.clientHeight * PixelRatio.get());
    canvas.width = width;
    canvas.height = height;
    const format = navigator.gpu.getPreferredCanvasFormat();
    context.configure({ device, format, alphaMode: "premultiplied" });

    const module = device.createShaderModule({ code: shader });
    const pipeline = device.createRenderPipeline({
      layout: "auto",
      vertex: { module, entryPoint: "vmain" },
      fragment: { module, entryPoint: "fmain", targets: [{ format }] },
      primitive: { topology: "triangle-list" },
    });
    const sampler = device.createSampler({
      magFilter: "linear",
      minFilter: "linear",
    });

    const tag = findNodeHandle(contentRef.current);
    if (tag == null) {
      setError("Could not resolve the content view's native tag");
      return;
    }

    const frame = async () => {
      if (cancelled) {
        return;
      }
      const size = contentSizeRef.current;
      if (!size) {
        // Content view not measured yet; try again next tick.
        raf = requestAnimationFrame(frame);
        return;
      }
      const dst = device.createTexture({
        size: [size.w, size.h],
        format: "rgba8unorm",
        usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.TEXTURE_BINDING,
      });
      try {
        // Surfaces async validation errors (e.g. a missing CopySrc usage on the
        // imported texture) that would otherwise be silent.
        device.pushErrorScope("validation");
        // Paint the native view into our texture.
        await device.queue.copyElementImageToTexture(
          { source: tag },
          { texture: dst },
        );
        const bindGroup = device.createBindGroup({
          layout: pipeline.getBindGroupLayout(0),
          entries: [
            { binding: 0, resource: sampler },
            { binding: 1, resource: dst.createView() },
          ],
        });
        const encoder = device.createCommandEncoder();
        const pass = encoder.beginRenderPass({
          colorAttachments: [
            {
              view: context.getCurrentTexture().createView(),
              clearValue: [0, 0, 0, 1],
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
        const gpuError = await device.popErrorScope();
        if (gpuError) {
          console.error("[LayoutSubtree] GPU validation:", gpuError.message);
          setError(gpuError.message);
          return;
        }
      } catch (e) {
        // Surface the real failure instead of silently losing the device.
        console.error("[LayoutSubtree] frame failed:", e);
        setError(String(e));
        return; // stop the loop; device stays retained by useDevice
      } finally {
        dst.destroy();
      }
      raf = requestAnimationFrame(frame);
    };
    frame();

    return () => {
      cancelled = true;
      cancelAnimationFrame(raf);
    };
  }, [device, ref]);

  return (
    <View style={styles.root}>
      <Canvas ref={ref} style={StyleSheet.absoluteFill} layoutSubtree>
        <View
          ref={contentRef}
          style={styles.content}
          collapsable={false}
          onLayout={(e) => {
            const { width: w, height: h } = e.nativeEvent.layout;
            contentSizeRef.current = {
              w: Math.max(1, Math.round(w * PixelRatio.get())),
              h: Math.max(1, Math.round(h * PixelRatio.get())),
            };
          }}
        >
          <Text style={styles.title}>Hello from a native View</Text>
          <View style={styles.swatch} />
          <Text style={styles.caption}>
            Rendered off-screen and sampled as a WebGPU texture
          </Text>
        </View>
      </Canvas>
      {error ? (
        <View style={styles.errorOverlay} pointerEvents="none">
          <Text style={styles.errorText}>{error}</Text>
        </View>
      ) : null}
    </View>
  );
}

const styles = StyleSheet.create({
  root: { flex: 1 },
  content: {
    flex: 1,
    alignItems: "center",
    justifyContent: "center",
    backgroundColor: "#1e293b",
    padding: 24,
  },
  title: { color: "#f8fafc", fontSize: 28, fontWeight: "700" },
  swatch: {
    width: 120,
    height: 120,
    borderRadius: 24,
    marginVertical: 24,
    backgroundColor: "#38bdf8",
  },
  caption: { color: "#94a3b8", fontSize: 14 },
  errorOverlay: {
    ...StyleSheet.absoluteFillObject,
    padding: 16,
    justifyContent: "flex-end",
  },
  errorText: { color: "#f87171", fontSize: 13 },
});
