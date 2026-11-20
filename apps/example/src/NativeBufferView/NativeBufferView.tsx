import React, { useEffect, useRef, useState } from "react";
import { Pressable, StyleSheet, Text, View } from "react-native";
import Animated, {
  useAnimatedStyle,
  useSharedValue,
  withTiming,
} from "react-native-reanimated";
import {
  createNativeBuffer,
  NativeBufferView as NativeBufferViewComponent,
  useNativeBufferSize,
  type NativeBufferPixelSize,
  type NativeBufferResizeMode,
  type NativeBufferViewRef,
} from "react-native-webgpu";

// A pattern that shows whether the buffer is displayed 1:1: one-pixel
// stripes and a one-pixel checkerboard turn to a flat grey as soon as the
// view scales the buffer, so a resize is visibly soft until the re-render
// lands. The rings are sized from the buffer, so a stretched buffer shows
// ellipses.
const SHADER = /* wgsl */ `
struct Uniforms {
  size: vec2f,
};
@group(0) @binding(0) var<uniform> u: Uniforms;

@vertex
fn vs(@builtin(vertex_index) i: u32) -> @builtin(position) vec4f {
  var p = array<vec2f, 3>(vec2f(-1.0, -3.0), vec2f(-1.0, 1.0), vec2f(3.0, 1.0));
  return vec4f(p[i], 0.0, 1.0);
}

@fragment
fn fs(@builtin(position) position: vec4f) -> @location(0) vec4f {
  let px = floor(position.xy);
  let uv = position.xy / u.size;
  var color = vec3f(uv.x, uv.y, 0.6);
  let center = u.size * 0.5;
  let radius = min(u.size.x, u.size.y) * 0.4;
  let d = distance(position.xy, center);
  if (d < radius && fract(d / (radius / 6.0)) < 0.5) {
    color = vec3f(1.0, 1.0, 1.0) - color;
  }
  let band = min(u.size.y * 0.15, 120.0);
  if (px.y < band) {
    // Alternating one-pixel columns on the left, a checkerboard on the right.
    var on = (u32(px.x) & 1u) == 0u;
    if (px.x > u.size.x * 0.5) {
      on = ((u32(px.x) + u32(px.y)) & 1u) == 0u;
    }
    color = select(vec3f(0.0), vec3f(1.0), on);
  }
  // A one-pixel red frame: missing edges mean the buffer is cropped.
  if (px.x < 1.0 || px.y < 1.0 || px.x >= u.size.x - 1.0 || px.y >= u.size.y - 1.0) {
    color = vec3f(1.0, 0.0, 0.0);
  }
  return vec4f(color, 1.0);
}
`;

// Renders one buffer of `size` with a device of its own, presents it, and
// releases everything: nothing GPU-related stays alive between renders.
const renderOnce = async (
  view: NativeBufferViewRef,
  size: NativeBufferPixelSize,
) => {
  const adapter = await navigator.gpu.requestAdapter();
  if (!adapter) {
    throw new Error("No adapter");
  }
  const device = await adapter.requestDevice();
  const frame = createNativeBuffer(size.width, size.height);
  const target = device.createNativeBufferTexture(frame, {
    usage: GPUTextureUsage.RENDER_ATTACHMENT,
  });
  try {
    const module = device.createShaderModule({ code: SHADER });
    const pipeline = device.createRenderPipeline({
      layout: "auto",
      vertex: { module },
      fragment: { module, targets: [{ format: target.texture.format }] },
    });
    const uniforms = device.createBuffer({
      size: 8,
      usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
    });
    device.queue.writeBuffer(
      uniforms,
      0,
      new Float32Array([size.width, size.height]),
    );
    const bindGroup = device.createBindGroup({
      layout: pipeline.getBindGroupLayout(0),
      entries: [{ binding: 0, resource: { buffer: uniforms } }],
    });

    target.beginAccess();
    const encoder = device.createCommandEncoder();
    const pass = encoder.beginRenderPass({
      colorAttachments: [
        {
          view: target.texture.createView(),
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
    // The view waits for this fence off the main thread before showing the
    // buffer, so nothing here waits for the GPU.
    const fence = target.endAccess();
    view.present(frame, fence);
  } finally {
    // The view holds its own reference on the buffer.
    target.destroy();
    frame.release();
    device.destroy();
  }
};

const RESIZE_MODES: NativeBufferResizeMode[] = [
  "stretch",
  "contain",
  "cover",
  "topLeft",
];

export const NativeBufferView = () => {
  const ref = useRef<NativeBufferViewRef>(null);
  const { size, onPixelSizeChange } = useNativeBufferSize(200);
  const [resizeMode, setResizeMode] =
    useState<NativeBufferResizeMode>("stretch");
  const [status, setStatus] = useState("Waiting for the first size");
  const [renders, setRenders] = useState(0);
  const expanded = useSharedValue(1);

  useEffect(() => {
    const view = ref.current;
    if (!size || !view) {
      return;
    }
    const start = performance.now();
    renderOnce(view, size)
      .then(() => {
        setRenders((n) => n + 1);
        setStatus(
          `${size.width}x${size.height} px rendered in ${Math.round(
            performance.now() - start,
          )} ms`,
        );
      })
      .catch((e: Error) => setStatus(`Error: ${e.message}`));
  }, [size]);

  const style = useAnimatedStyle(() => ({
    width: `${50 + 50 * expanded.value}%`,
    height: 160 + 240 * expanded.value,
  }));

  return (
    <View style={styles.container}>
      <View style={styles.stage}>
        <Animated.View style={style}>
          <NativeBufferViewComponent
            ref={ref}
            style={StyleSheet.absoluteFill}
            resizeMode={resizeMode}
            onPixelSizeChange={onPixelSizeChange}
          />
        </Animated.View>
      </View>
      <Text style={styles.status}>
        {status} ({renders} {renders === 1 ? "render" : "renders"})
      </Text>
      <View style={styles.row}>
        <Pressable
          style={styles.button}
          onPress={() => {
            expanded.value = withTiming(expanded.value > 0.5 ? 0 : 1, {
              duration: 800,
            });
          }}
        >
          <Text style={styles.buttonText}>Resize</Text>
        </Pressable>
      </View>
      <View style={styles.row}>
        {RESIZE_MODES.map((mode) => (
          <Pressable
            key={mode}
            style={[styles.button, mode === resizeMode && styles.selected]}
            onPress={() => setResizeMode(mode)}
          >
            <Text style={styles.buttonText}>{mode}</Text>
          </Pressable>
        ))}
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: "white",
  },
  stage: {
    height: 420,
    alignItems: "center",
    justifyContent: "center",
    backgroundColor: "#eee",
  },
  status: {
    margin: 16,
    color: "black",
  },
  row: {
    flexDirection: "row",
    flexWrap: "wrap",
    marginHorizontal: 12,
  },
  button: {
    paddingHorizontal: 12,
    paddingVertical: 8,
    margin: 4,
    borderRadius: 6,
    backgroundColor: "#ddd",
  },
  selected: {
    backgroundColor: "#9cf",
  },
  buttonText: {
    color: "black",
  },
});
