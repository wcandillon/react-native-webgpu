import { useEffect, useRef, useState } from "react";
import {
  findNodeHandle,
  PixelRatio,
  Platform,
  Pressable,
  ScrollView,
  StyleSheet,
  Text,
  View,
} from "react-native";
import type { CanvasRef } from "react-native-webgpu";
import { Canvas, useCanvasRef, useDevice } from "react-native-webgpu";

const ROWS = Array.from({ length: 40 }, (_, i) => i);
const COLORS = ["#1e293b", "#334155"];

// Benchmark screen for "HTML in Canvas": captures the native view on EVERY
// frame (no dirty-window throttling) so we can measure how fast
// copyElementImageToTexture can actually go. The overlay reports a rolling
// average of the capture latency, the full frame time, and the resulting fps.
// Tap the overlay to toggle the per-frame error-scope sync: with it on you see
// the real-world cost (a CPU<->GPU round-trip every capture), with it off you
// see the raw capture throughput.

const now = (): number =>
  typeof globalThis.performance?.now === "function"
    ? globalThis.performance.now()
    : Date.now();

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

interface Stats {
  capMs: number;
  frameMs: number;
  fps: number;
}

const useBenchmarkCapture = ({
  canvasRef,
  contentRef,
  contentSizeRef,
  syncRef,
  setStats,
  setError,
}: {
  canvasRef: React.RefObject<CanvasRef | null>;
  contentRef: React.RefObject<View | null>;
  contentSizeRef: React.RefObject<{ w: number; h: number } | null>;
  syncRef: React.RefObject<boolean>;
  setStats: (s: Stats) => void;
  setError: (m: string | null) => void;
}) => {
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

    const capturedFormat: GPUTextureFormat =
      Platform.OS === "ios" ? "bgra8unorm" : "rgba8unorm";

    let dst: GPUTexture | null = null;
    let dstW = 0;
    let dstH = 0;
    let hasCaptured = false;

    // Rolling averages over a window of frames so the overlay is readable.
    const WINDOW = 30;
    let capAcc = 0;
    let frameAcc = 0;
    let samples = 0;

    const frame = async () => {
      if (cancelled) {
        return;
      }
      const frameStart = now();
      const size = contentSizeRef.current;
      const tag = size ? findNodeHandle(contentRef.current) : null;

      let capMs = 0;
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
          hasCaptured = false;
        }
        try {
          const capStart = now();
          if (syncRef.current) {
            device.pushErrorScope("validation");
          }
          await device.queue.copyElementImageToTexture(
            { source: tag },
            { texture: dst },
          );
          if (syncRef.current) {
            const gpuError = await device.popErrorScope();
            if (gpuError) {
              setError(gpuError.message);
              return;
            }
          }
          capMs = now() - capStart;
          hasCaptured = true;
        } catch (e) {
          setError(String(e));
          return;
        }
      }

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
            ],
          }),
        );
        pass.draw(3);
      }
      pass.end();
      device.queue.submit([encoder.finish()]);
      context.present();

      const frameMs = now() - frameStart;
      capAcc += capMs;
      frameAcc += frameMs;
      samples += 1;
      if (samples >= WINDOW) {
        const avgFrame = frameAcc / samples;
        setStats({
          capMs: capAcc / samples,
          frameMs: avgFrame,
          fps: avgFrame > 0 ? 1000 / avgFrame : 0,
        });
        capAcc = 0;
        frameAcc = 0;
        samples = 0;
      }

      raf = requestAnimationFrame(frame);
    };
    frame();

    return () => {
      cancelled = true;
      cancelAnimationFrame(raf);
      dst?.destroy();
    };
  }, [device, canvasRef, contentRef, contentSizeRef, syncRef, setStats, setError]);
};

export function LayoutSubtreeBenchmark() {
  const ref = useCanvasRef();
  const contentRef = useRef<View>(null);
  const contentSizeRef = useRef<{ w: number; h: number } | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [stats, setStats] = useState<Stats | null>(null);
  const [sync, setSync] = useState(true);
  const syncRef = useRef(sync);
  syncRef.current = sync;

  useBenchmarkCapture({
    canvasRef: ref,
    contentRef,
    contentSizeRef,
    syncRef,
    setStats,
    setError,
  });

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
          <ScrollView>
            {ROWS.map((i) => (
              <View
                key={i}
                style={[styles.row, { backgroundColor: COLORS[i % 2] }]}
              >
                <Text style={styles.rowText}>Benchmark row #{i}</Text>
              </View>
            ))}
          </ScrollView>
        </View>
      </Canvas>
      <Pressable
        style={styles.overlay}
        onPress={() => setSync((s) => !s)}
        pointerEvents="box-only"
      >
        <Text style={styles.statText}>
          capture: {stats ? stats.capMs.toFixed(2) : "--"} ms
        </Text>
        <Text style={styles.statText}>
          frame: {stats ? stats.frameMs.toFixed(2) : "--"} ms
        </Text>
        <Text style={styles.statText}>
          fps: {stats ? stats.fps.toFixed(0) : "--"}
        </Text>
        <Text style={styles.statHint}>
          error-scope sync: {sync ? "ON (real-world)" : "OFF (raw)"} — tap to
          toggle
        </Text>
      </Pressable>
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
  content: { flex: 1, backgroundColor: "#0f172a" },
  row: {
    height: 72,
    justifyContent: "center",
    paddingHorizontal: 24,
  },
  rowText: { color: "#f8fafc", fontSize: 20, fontWeight: "600" },
  overlay: {
    position: "absolute",
    top: 48,
    left: 16,
    padding: 12,
    borderRadius: 8,
    backgroundColor: "rgba(0,0,0,0.6)",
  },
  statText: {
    color: "#4ade80",
    fontSize: 16,
    fontWeight: "700",
    fontVariant: ["tabular-nums"],
  },
  statHint: { color: "#cbd5e1", fontSize: 11, marginTop: 6 },
  errorOverlay: {
    ...StyleSheet.absoluteFillObject,
    padding: 16,
    justifyContent: "flex-end",
  },
  errorText: { color: "#f87171", fontSize: 13 },
});
