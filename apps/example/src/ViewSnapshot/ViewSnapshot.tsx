import React, { useCallback, useEffect, useRef, useState } from "react";
import {
  Image,
  PixelRatio,
  Pressable,
  StyleSheet,
  Text,
  View,
} from "react-native";
import { Canvas, useCanvasRef, useDevice } from "react-native-webgpu";

// Draws the snapshot texture with a gentle animated ripple, so it is obvious
// that the native view has become a regular sampled GPU texture.
const SHADER = /* wgsl */ `
struct Uniforms {
  time: f32,
};

@group(0) @binding(0) var tex: texture_2d<f32>;
@group(0) @binding(1) var texSampler: sampler;
@group(0) @binding(2) var<uniform> u: Uniforms;

struct VsOut {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
};

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
  let wave = sin(in.uv.y * 24.0 + u.time * 4.0) * 0.01;
  let uv = vec2f(in.uv.x + wave, in.uv.y);
  return textureSample(tex, texSampler, uv);
}
`;

const CARD_WIDTH = 320;
const CARD_HEIGHT = 200;

// The "element": a plain React Native view hierarchy. collapsable={false}
// keeps the host view around so it can be looked up and rasterized.
const Card = React.forwardRef<View, { count: number }>(({ count }, ref) => (
  <View ref={ref} collapsable={false} style={styles.card}>
    <View style={styles.cardHeader}>
      <Image
        source={require("../assets/react.png")}
        style={styles.logo}
        resizeMode="contain"
      />
      <Text style={styles.title}>Hello from UIKit / Android Views</Text>
    </View>
    <Text style={styles.body}>
      This card is a regular React Native view. Tap "Snapshot" to copy it into a
      GPUTexture with queue.drawElementImageToTexture and sample it below.
    </Text>
    <View style={styles.row}>
      {["#61dafb", "#f5a623", "#7ed321", "#bd10e0"].map((color, i) => (
        <View
          key={color}
          style={[
            styles.chip,
            { backgroundColor: color, transform: [{ rotate: `${i * 8}deg` }] },
          ]}
        />
      ))}
      <Text style={styles.counter}>{count}</Text>
    </View>
  </View>
));

export const ViewSnapshot = () => {
  const { device } = useDevice();
  const canvasRef = useCanvasRef();
  const cardRef = useRef<View>(null);
  const [count, setCount] = useState(0);
  const [texture, setTexture] = useState<GPUTexture | null>(null);
  const [error, setError] = useState<string | null>(null);

  const snapshot = useCallback(async () => {
    if (!device) {
      return;
    }
    try {
      // Natural pixel size: layout points times the device pixel ratio.
      const scale = PixelRatio.get();
      const tex = device.createTexture({
        size: [Math.round(CARD_WIDTH * scale), Math.round(CARD_HEIGHT * scale)],
        format: "rgba8unorm",
        usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.TEXTURE_BINDING,
      });
      await device.queue.drawElementImageToTexture(
        { source: cardRef },
        { texture: tex },
      );
      setTexture((previous) => {
        previous?.destroy();
        return tex;
      });
      setError(null);
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    }
  }, [device]);

  // Take a first snapshot once the device is ready and the card is laid out.
  useEffect(() => {
    if (device) {
      const handle = setTimeout(snapshot, 100);
      return () => clearTimeout(handle);
    }
    return undefined;
  }, [device, snapshot]);

  useEffect(() => {
    if (!device || !texture) {
      return;
    }
    const context = canvasRef.current!.getContext("webgpu")!;
    const canvas = context.canvas as HTMLCanvasElement;
    canvas.width = canvas.clientWidth * PixelRatio.get();
    canvas.height = canvas.clientHeight * PixelRatio.get();
    const format = navigator.gpu.getPreferredCanvasFormat();
    context.configure({ device, format, alphaMode: "opaque" });

    const module = device.createShaderModule({ code: SHADER });
    const pipeline = device.createRenderPipeline({
      layout: "auto",
      vertex: { module, entryPoint: "vs_main" },
      fragment: { module, entryPoint: "fs_main", targets: [{ format }] },
      primitive: { topology: "triangle-list" },
    });
    const sampler = device.createSampler({
      magFilter: "linear",
      minFilter: "linear",
      addressModeU: "clamp-to-edge",
      addressModeV: "clamp-to-edge",
    });
    const uniforms = device.createBuffer({
      size: 16,
      usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
    });
    const bindGroup = device.createBindGroup({
      layout: pipeline.getBindGroupLayout(0),
      entries: [
        { binding: 0, resource: texture.createView() },
        { binding: 1, resource: sampler },
        { binding: 2, resource: { buffer: uniforms } },
      ],
    });

    let frame = 0;
    const start = Date.now();
    const render = () => {
      device.queue.writeBuffer(
        uniforms,
        0,
        new Float32Array([(Date.now() - start) / 1000, 0, 0, 0]),
      );
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
      frame = requestAnimationFrame(render);
    };
    frame = requestAnimationFrame(render);
    return () => {
      cancelAnimationFrame(frame);
      uniforms.destroy();
    };
  }, [device, texture, canvasRef]);

  return (
    <View style={styles.container}>
      <Card ref={cardRef} count={count} />
      <View style={styles.buttons}>
        <Pressable
          style={styles.button}
          onPress={() => setCount((value) => value + 1)}
        >
          <Text style={styles.buttonLabel}>Change the view</Text>
        </Pressable>
        <Pressable style={styles.button} onPress={snapshot}>
          <Text style={styles.buttonLabel}>Snapshot</Text>
        </Pressable>
      </View>
      {error ? <Text style={styles.error}>{error}</Text> : null}
      <Canvas
        ref={canvasRef}
        style={{ width: CARD_WIDTH, height: CARD_HEIGHT }}
      />
      <Text style={styles.caption}>
        The canvas above samples the snapshot GPUTexture (with a ripple).
      </Text>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    alignItems: "center",
    paddingTop: 24,
    gap: 16,
    backgroundColor: "#f4f4f6",
  },
  card: {
    width: CARD_WIDTH,
    height: CARD_HEIGHT,
    borderRadius: 16,
    backgroundColor: "#20232a",
    padding: 16,
    justifyContent: "space-between",
    overflow: "hidden",
  },
  cardHeader: {
    flexDirection: "row",
    alignItems: "center",
    gap: 12,
  },
  logo: {
    width: 40,
    height: 40,
  },
  title: {
    flex: 1,
    color: "#61dafb",
    fontSize: 17,
    fontWeight: "700",
  },
  body: {
    color: "#e8e8e8",
    fontSize: 13,
    lineHeight: 18,
  },
  row: {
    flexDirection: "row",
    alignItems: "center",
    gap: 10,
  },
  chip: {
    width: 28,
    height: 28,
    borderRadius: 6,
  },
  counter: {
    marginLeft: "auto",
    color: "white",
    fontSize: 28,
    fontWeight: "800",
  },
  buttons: {
    flexDirection: "row",
    gap: 12,
  },
  button: {
    paddingHorizontal: 16,
    paddingVertical: 10,
    borderRadius: 8,
    backgroundColor: "#0a84ff",
  },
  buttonLabel: {
    color: "white",
    fontWeight: "600",
  },
  error: {
    color: "red",
    paddingHorizontal: 16,
  },
  caption: {
    color: "#666",
    fontSize: 12,
  },
});
