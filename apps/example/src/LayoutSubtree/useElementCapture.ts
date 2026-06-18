import type { RefObject } from "react";
import { useEffect } from "react";
import type { View } from "react-native";
import { findNodeHandle, PixelRatio, Platform } from "react-native";
import type { CanvasRef } from "react-native-webgpu";
import { useDevice } from "react-native-webgpu";

// Samples the captured element texture onto a full-screen triangle with a
// time-driven wobble + tint, so it's obvious this is a processed GPU texture
// and not the live native view underneath.
const shader = /* wgsl */ `
struct VSOut {
  @builtin(position) pos: vec4f,
  @location(0) uv: vec2f,
};

struct Uniforms { time: f32 };

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
@group(0) @binding(2) var<uniform> u: Uniforms;

@fragment
fn fmain(in: VSOut) -> @location(0) vec4f {
  var uv = in.uv;
  uv.x += sin(in.uv.y * 24.0 + u.time * 2.0) * 0.008;
  uv.y += sin(in.uv.x * 24.0 + u.time * 1.6) * 0.008;
  var color = textureSample(tex, samp, uv);
  let tint = 0.5 + 0.5 * sin(u.time + in.uv.x * 3.0 + vec3f(0.0, 2.0, 4.0));
  color = vec4f(mix(color.rgb, color.rgb * tint, 0.4), color.a);
  return color;
}`;

interface Params {
  canvasRef: RefObject<CanvasRef | null>;
  // The native child view to capture.
  contentRef: RefObject<View | null>;
  // The content's pixel size, kept current by the caller's onLayout.
  contentSizeRef: RefObject<{ w: number; h: number } | null>;
  // Returns true on frames where the content may have changed and should be
  // re-captured. The first frame (and any resize) always captures regardless.
  shouldCaptureRef: RefObject<() => boolean>;
  setError: (message: string | null) => void;
}

// Drives a capture-on-change render loop: it re-captures the element only when
// `shouldCaptureRef` says the content is changing (and once up front / on
// resize), and re-runs the shader on the cached texture every frame. This makes
// static content cost one capture, while active content (scrolling, editing)
// captures per frame only for the duration of the activity.
export const useElementCapture = ({
  canvasRef,
  contentRef,
  contentSizeRef,
  shouldCaptureRef,
  setError,
}: Params) => {
  const { device } = useDevice();

  useEffect(() => {
    if (!device) {
      return;
    }
    let raf = 0;
    let cancelled = false;

    const context = canvasRef.current?.getContext("webgpu");
    if (!context) {
      return;
    }
    const canvas = context.canvas as HTMLCanvasElement;
    const format = navigator.gpu.getPreferredCanvasFormat();
    canvas.width = Math.floor(canvas.clientWidth * PixelRatio.get());
    canvas.height = Math.floor(canvas.clientHeight * PixelRatio.get());
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
    const uniformBuffer = device.createBuffer({
      size: 16,
      usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
    });

    // The captured surface is RGBA on Android (AHardwareBuffer) and BGRA on iOS
    // (IOSurface); the destination texture mirrors it so copyTextureToTexture
    // formats match.
    const capturedFormat: GPUTextureFormat =
      Platform.OS === "ios" ? "bgra8unorm" : "rgba8unorm";

    let dst: GPUTexture | null = null;
    let dstW = 0;
    let dstH = 0;
    let hasCaptured = false;
    let time = 0;

    const frame = async () => {
      if (cancelled) {
        return;
      }
      const size = contentSizeRef.current;
      const tag = size ? findNodeHandle(contentRef.current) : null;

      if (size && tag != null) {
        if (!dst || dstW !== size.w || dstH !== size.h) {
          dst?.destroy();
          dst = device.createTexture({
            size: [size.w, size.h],
            format: capturedFormat,
            usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.TEXTURE_BINDING,
          });
          dstW = size.w;
          dstH = size.h;
          hasCaptured = false; // a fresh texture must be (re)filled
        }
        if (!hasCaptured || shouldCaptureRef.current?.()) {
          try {
            device.pushErrorScope("validation");
            await device.queue.copyElementImageToTexture(
              { source: tag },
              { texture: dst },
            );
            hasCaptured = true;
            const gpuError = await device.popErrorScope();
            if (gpuError) {
              setError(gpuError.message);
              return;
            }
          } catch (e) {
            setError(String(e));
            return;
          }
        }
      }

      time += 1 / 60;
      device.queue.writeBuffer(uniformBuffer, 0, new Float32Array([time]));
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
      if (dst && hasCaptured) {
        pass.setPipeline(pipeline);
        pass.setBindGroup(
          0,
          device.createBindGroup({
            layout: pipeline.getBindGroupLayout(0),
            entries: [
              { binding: 0, resource: sampler },
              { binding: 1, resource: dst.createView() },
              { binding: 2, resource: { buffer: uniformBuffer } },
            ],
          }),
        );
        pass.draw(3);
      }
      pass.end();
      device.queue.submit([encoder.finish()]);
      context.present();
      raf = requestAnimationFrame(frame);
    };
    frame();

    return () => {
      cancelled = true;
      cancelAnimationFrame(raf);
      dst?.destroy();
    };
  }, [
    device,
    canvasRef,
    contentRef,
    contentSizeRef,
    shouldCaptureRef,
    setError,
  ]);
};
