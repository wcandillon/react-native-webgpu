import React, { useEffect, useMemo, useRef, useState } from "react";
import { Pressable, StyleSheet, Text, View } from "react-native";
import type { CanvasRef, RNCanvasContext } from "react-native-webgpu";
import {
  Canvas,
  GPUBufferUsage,
  GPUTextureUsage,
  requestAnimationFrame,
} from "react-native-webgpu";
import type { SharedValue } from "react-native-reanimated";
import { useSharedValue } from "react-native-reanimated";
import { useSafeAreaInsets } from "react-native-safe-area-context";
import {
  createWorkletRuntime,
  scheduleOnRuntime,
  scheduleOnUI,
  type WorkletRuntime,
} from "react-native-worklets";

import { cubeVertexArray } from "../components/cube";
import { basicVertWGSL, vertexPositionColorWGSL } from "../Cube/Shaders";

// Renders a cube from a requestAnimationFrame loop, on the runtime and with
// the requestAnimationFrame picked below, and shows the frames per second it
// presents. "React Native" is the runtime's global requestAnimationFrame (a
// timer on the React Native runtime, worklets' on the others); "WebGPU" is
// the one of react-native-webgpu, paced by the display. "JS work" adds that
// much busy work to every frame.
//
// The canvas fills the screen, under the controls: on Android 15+, a window
// whose updates cover less than about half of the display is "small dirty"
// to SurfaceFlinger, which then runs the whole app at 60 Hz whatever paces it.

type RuntimeChoice = "js" | "ui" | "worklet";
type RafChoice = "global" | "webgpu";

const cubeLoop = (
  running: SharedValue<boolean>,
  fps: SharedValue<number>,
  workMs: number,
  useWebGPURaf: boolean,
  device: GPUDevice,
  context: RNCanvasContext,
  format: GPUTextureFormat,
  vertices: GPUBuffer,
) => {
  "worklet";
  const raf = useWebGPURaf
    ? requestAnimationFrame
    : globalThis.requestAnimationFrame;
  context.configure({ device, format, alphaMode: "premultiplied" });
  const pipeline = device.createRenderPipeline({
    layout: "auto",
    vertex: {
      module: device.createShaderModule({ code: basicVertWGSL }),
      buffers: [
        {
          arrayStride: 40,
          attributes: [
            { shaderLocation: 0, offset: 0, format: "float32x4" },
            { shaderLocation: 1, offset: 32, format: "float32x2" },
          ],
        },
      ],
    },
    fragment: {
      module: device.createShaderModule({ code: vertexPositionColorWGSL }),
      targets: [{ format }],
    },
    primitive: { topology: "triangle-list", cullMode: "back" },
    depthStencil: {
      depthWriteEnabled: true,
      depthCompare: "less",
      format: "depth24plus",
    },
  });
  const uniforms = device.createBuffer({
    size: 64,
    usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
  });
  const bindGroup = device.createBindGroup({
    layout: pipeline.getBindGroupLayout(0),
    entries: [{ binding: 0, resource: { buffer: uniforms } }],
  });
  const mvp = new Float32Array(16);
  let depth: GPUTexture | null = null;
  // Counted here, on the loop's own runtime, and published once a second: a
  // shared value read or written on every frame would cost a hop to the UI
  // runtime each time.
  let frames = 0;
  let windowStart = performance.now();

  // projection * translate(0, 0, -4) * rotate(axis(t), 1), column-major.
  const updateMatrix = (aspect: number, seconds: number) => {
    const f = 1 / Math.tan(Math.PI / 5);
    const near = 1;
    const far = 100;
    const ri = 1 / (near - far);
    const p = [
      f / aspect,
      0,
      0,
      0,
      0,
      f,
      0,
      0,
      0,
      0,
      far * ri,
      -1,
      0,
      0,
      near * far * ri,
      0,
    ];
    const x = Math.sin(seconds);
    const y = Math.cos(seconds);
    const c = Math.cos(1);
    const s = Math.sin(1);
    const t = 1 - c;
    const v = [
      t * x * x + c,
      t * x * y,
      -s * y,
      0,
      t * x * y,
      t * y * y + c,
      s * x,
      0,
      s * y,
      -s * x,
      c,
      0,
      0,
      0,
      -4,
      1,
    ];
    for (let col = 0; col < 4; col++) {
      for (let row = 0; row < 4; row++) {
        let sum = 0;
        for (let k = 0; k < 4; k++) {
          sum += p[k * 4 + row] * v[col * 4 + k];
        }
        mvp[col * 4 + row] = sum;
      }
    }
  };

  const frame = (timestamp: number) => {
    if (!running.value) {
      return;
    }
    const end = performance.now() + workMs;
    while (performance.now() < end) {
      // Busy work, to see how much of the frame the loop can use.
    }
    const texture = context.getCurrentTexture();
    if (depth === null) {
      depth = device.createTexture({
        size: [texture.width, texture.height],
        format: "depth24plus",
        usage: GPUTextureUsage.RENDER_ATTACHMENT,
      });
    }
    updateMatrix(texture.width / texture.height, timestamp / 1000);
    device.queue.writeBuffer(uniforms, 0, mvp.buffer, 0, 64);
    const encoder = device.createCommandEncoder();
    const pass = encoder.beginRenderPass({
      colorAttachments: [
        {
          view: texture.createView(),
          clearValue: [0, 0, 0, 0],
          loadOp: "clear",
          storeOp: "store",
        },
      ],
      depthStencilAttachment: {
        view: depth.createView(),
        depthClearValue: 1,
        depthLoadOp: "clear",
        depthStoreOp: "store",
      },
    });
    pass.setPipeline(pipeline);
    pass.setBindGroup(0, bindGroup);
    pass.setVertexBuffer(0, vertices);
    pass.draw(36);
    pass.end();
    device.queue.submit([encoder.finish()]);
    context.present();
    frames++;
    const now = performance.now();
    if (now - windowStart >= 1000) {
      fps.value = (frames * 1000) / (now - windowStart);
      frames = 0;
      windowStart = now;
    }
    raf(frame);
  };
  raf(frame);
};

const Segmented = <T extends string | number>({
  label,
  options,
  value,
  onChange,
}: {
  label: string;
  options: { value: T; label: string }[];
  value: T;
  onChange: (value: T) => void;
}) => (
  <View style={styles.row}>
    <Text style={styles.label}>{label}</Text>
    <View style={styles.segmented}>
      {options.map((option) => {
        const selected = option.value === value;
        return (
          <Pressable
            key={String(option.value)}
            onPress={() => onChange(option.value)}
            style={[styles.segment, selected && styles.segmentSelected]}
          >
            <Text
              style={[
                styles.segmentText,
                selected && styles.segmentTextSelected,
              ]}
            >
              {option.label}
            </Text>
          </Pressable>
        );
      })}
    </View>
  </View>
);

const CubeCanvas = ({
  runtime,
  raf,
  workMs,
  fps,
  workletRuntime,
}: {
  runtime: RuntimeChoice;
  raf: RafChoice;
  workMs: number;
  fps: SharedValue<number>;
  workletRuntime: WorkletRuntime;
}) => {
  const ref = useRef<CanvasRef>(null);
  const running = useSharedValue(true);
  useEffect(() => {
    let cancelled = false;
    (async () => {
      const adapter = await navigator.gpu.requestAdapter();
      const device = await adapter?.requestDevice();
      const context = ref.current?.getContext("webgpu");
      if (!device || !context || cancelled) {
        return;
      }
      const vertices = device.createBuffer({
        size: cubeVertexArray.byteLength,
        usage: GPUBufferUsage.VERTEX,
        mappedAtCreation: true,
      });
      new Float32Array(vertices.getMappedRange()).set(cubeVertexArray);
      vertices.unmap();
      const format = navigator.gpu.getPreferredCanvasFormat();
      const args = [
        running,
        fps,
        workMs,
        raf === "webgpu",
        device,
        context,
        format,
        vertices,
      ] as const;
      if (runtime === "js") {
        cubeLoop(...args);
      } else if (runtime === "ui") {
        scheduleOnUI(cubeLoop, ...args);
      } else {
        scheduleOnRuntime(workletRuntime, cubeLoop, ...args);
      }
    })();
    return () => {
      cancelled = true;
      running.value = false;
    };
  }, [runtime, raf, workMs, fps, running, workletRuntime]);
  return <Canvas ref={ref} style={styles.canvas} />;
};

export const FramePacing = () => {
  const [runtime, setRuntime] = useState<RuntimeChoice>("js");
  const [raf, setRaf] = useState<RafChoice>("webgpu");
  const [work, setWork] = useState(0);
  const [fpsText, setFpsText] = useState(0);
  const fps = useSharedValue(0);
  // One worklet runtime for the screen, not one per canvas: the canvas
  // remounts at every change.
  const workletRuntime = useMemo(
    () => createWorkletRuntime({ name: "FramePacing" }),
    [],
  );
  const insets = useSafeAreaInsets();

  useEffect(() => {
    const interval = setInterval(() => setFpsText(fps.value), 500);
    return () => clearInterval(interval);
  }, [fps]);

  return (
    <View style={styles.container}>
      <CubeCanvas
        key={`${runtime}-${raf}-${work}`}
        runtime={runtime}
        raf={raf}
        workMs={work}
        fps={fps}
        workletRuntime={workletRuntime}
      />
      <View style={[styles.panel, { paddingBottom: 16 + insets.bottom }]}>
        <Text style={styles.fps}>{fpsText.toFixed(0)} fps</Text>
        <Segmented
          label="Runtime"
          value={runtime}
          onChange={setRuntime}
          options={[
            { value: "js", label: "JS thread" },
            { value: "ui", label: "UI" },
            { value: "worklet", label: "Worklet" },
          ]}
        />
        <Segmented
          label="requestAnimationFrame"
          value={raf}
          onChange={setRaf}
          options={[
            { value: "global", label: "React Native" },
            { value: "webgpu", label: "WebGPU" },
          ]}
        />
        <Segmented
          label="JS work per frame"
          value={work}
          onChange={setWork}
          options={[
            { value: 0, label: "0 ms" },
            { value: 5, label: "5 ms" },
            { value: 7, label: "7 ms" },
            { value: 10, label: "10 ms" },
          ]}
        />
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: "#F2F2F7",
  },
  canvas: {
    position: "absolute",
    top: 0,
    left: 0,
    right: 0,
    bottom: 0,
  },
  panel: {
    position: "absolute",
    left: 0,
    right: 0,
    bottom: 0,
    padding: 16,
    gap: 12,
    backgroundColor: "white",
    borderTopWidth: StyleSheet.hairlineWidth,
    borderTopColor: "#C6C6C8",
  },
  fps: {
    fontSize: 34,
    fontWeight: "700",
    fontVariant: ["tabular-nums"],
    color: "#000",
  },
  row: {
    gap: 6,
  },
  label: {
    fontSize: 13,
    color: "#6D6D72",
  },
  segmented: {
    flexDirection: "row",
    backgroundColor: "#EEEEF0",
    borderRadius: 9,
    padding: 2,
  },
  segment: {
    flex: 1,
    paddingVertical: 6,
    alignItems: "center",
    borderRadius: 7,
  },
  segmentSelected: {
    backgroundColor: "white",
    shadowColor: "#000",
    shadowOpacity: 0.12,
    shadowRadius: 4,
    shadowOffset: { width: 0, height: 1 },
    elevation: 2,
  },
  segmentText: {
    fontSize: 13,
    color: "#000",
  },
  segmentTextSelected: {
    fontWeight: "600",
  },
});
