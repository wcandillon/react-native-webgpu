import React, { useCallback, useEffect, useRef, useState } from "react";
import {
  Button,
  Platform,
  ScrollView,
  Settings,
  StyleSheet,
  Text,
  View,
} from "react-native";
import type { CanvasRef, RNCanvasContext } from "react-native-webgpu";
import { Canvas, installWebGPU } from "react-native-webgpu";
import { runOnUI, useSharedValue } from "react-native-reanimated";
import type { SharedValue } from "react-native-reanimated";

const MODES = [
  "write",
  "submit",
  "create",
  "map",
  "present",
  "churn",
  "destroy",
  "all",
] as const;

type ReproMode = (typeof MODES)[number];

const MODE_SETTING = "RNWebGPUCrossRuntimeMode";

const OPS_PER_FRAME = 32;
const OPS_PER_TICK = 32;
const SURFACE_CHURN_INTERVAL_MS = 150;
const DESTROY_AFTER_MS = 2_000;

interface Shared {
  device: GPUDevice;
  format: GPUTextureFormat;
  uiBuffer: GPUBuffer;
  jsBuffer: GPUBuffer;
}

const readInitialMode = (): ReproMode => {
  if (Platform.OS !== "ios") {
    return "write";
  }
  const stored = Settings.get(MODE_SETTING);
  return MODES.includes(stored) ? (stored as ReproMode) : "write";
};

const renderOnUI = (
  running: SharedValue<boolean>,
  uiFrames: SharedValue<number>,
  device: GPUDevice,
  context: RNCanvasContext,
  buffer: GPUBuffer,
  format: GPUTextureFormat,
  mode: ReproMode,
  ops: number,
) => {
  "worklet";

  installWebGPU();
  context.configure({ device, format, alphaMode: "premultiplied" });

  const data = new Float32Array(4);
  let frameNumber = 0;

  const stress = () => {
    if (mode === "write" || mode === "all") {
      for (let i = 0; i < ops; i += 1) {
        data[1] = i;
        device.queue.writeBuffer(buffer, 0, data);
      }
    }
    if (mode === "submit" || mode === "all") {
      for (let i = 0; i < ops; i += 1) {
        device.queue.submit([device.createCommandEncoder().finish()]);
      }
    }
    if (mode === "create" || mode === "all") {
      for (let i = 0; i < ops; i += 1) {
        const scratch = device.createBuffer({
          size: 256,
          usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.UNIFORM,
        });
        scratch.destroy();
      }
    }
  };

  const frame = () => {
    if (!running.value) {
      return;
    }
    frameNumber += 1;
    uiFrames.value = frameNumber;
    data[0] = frameNumber;
    stress();

    const encoder = device.createCommandEncoder();
    const pass = encoder.beginRenderPass({
      colorAttachments: [
        {
          view: context.getCurrentTexture().createView(),
          clearValue: [
            0.5 + 0.5 * Math.sin(frameNumber / 29),
            0.5 + 0.5 * Math.sin(frameNumber / 31 + 2),
            0.5 + 0.5 * Math.sin(frameNumber / 37 + 4),
            1,
          ],
          loadOp: "clear",
          storeOp: "store",
        },
      ],
    });
    pass.end();
    device.queue.submit([encoder.finish()]);
    context.present();

    requestAnimationFrame(frame);
  };

  frame();
};

const RenderCanvas = ({
  shared,
  mode,
  uiFrames,
}: {
  shared: Shared;
  mode: ReproMode;
  uiFrames: SharedValue<number>;
}) => {
  const ref = useRef<CanvasRef>(null);
  const running = useSharedValue(true);

  useEffect(() => {
    const context = ref.current?.getContext("webgpu");
    if (!context) {
      throw new Error("Failed to create the WebGPU canvas context");
    }
    running.value = true;
    runOnUI(renderOnUI)(
      running,
      uiFrames,
      shared.device,
      context,
      shared.uiBuffer,
      shared.format,
      mode,
      mode === "present" || mode === "churn" ? 0 : OPS_PER_FRAME,
    );
    return () => {
      running.value = false;
    };
  }, [shared, running, mode, uiFrames]);

  return <Canvas ref={ref} style={styles.canvas} />;
};

const ChurnCanvas = ({ shared }: { shared: Shared }) => {
  const ref = useRef<CanvasRef>(null);

  useEffect(() => {
    const context = ref.current?.getContext("webgpu");
    if (!context) {
      return;
    }
    context.configure({
      device: shared.device,
      format: shared.format,
      alphaMode: "premultiplied",
    });
    const encoder = shared.device.createCommandEncoder();
    const pass = encoder.beginRenderPass({
      colorAttachments: [
        {
          view: context.getCurrentTexture().createView(),
          clearValue: [1, 0, 1, 1],
          loadOp: "clear",
          storeOp: "store",
        },
      ],
    });
    pass.end();
    shared.device.queue.submit([encoder.finish()]);
    context.present();
  }, [shared]);

  return <Canvas ref={ref} style={styles.churn} />;
};

export const CrossRuntimeDevice = () => {
  const [mode, setMode] = useState<ReproMode>(readInitialMode);
  const [shared, setShared] = useState<Shared | null>(null);
  const [churnEpoch, setChurnEpoch] = useState(0);
  const [ticks, setTicks] = useState(0);
  const [uiFrameCount, setUiFrameCount] = useState(0);
  const uiFrames = useSharedValue(0);
  const jsTicks = useRef(0);

  useEffect(() => {
    if (Platform.OS === "ios") {
      Settings.set({ [MODE_SETTING]: mode });
    }
  }, [mode]);

  useEffect(() => {
    let live = true;
    let created: Shared | null = null;

    (async () => {
      installWebGPU();
      const adapter = await navigator.gpu.requestAdapter();
      if (!adapter) {
        throw new Error("No WebGPU adapter");
      }
      const device = await adapter.requestDevice();
      const makeBuffer = (label: string) =>
        device.createBuffer({
          label,
          size: 16,
          usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.UNIFORM,
        });
      created = {
        device,
        format: navigator.gpu.getPreferredCanvasFormat(),
        uiBuffer: makeBuffer("ui-runtime uniform"),
        jsBuffer: makeBuffer("js-runtime uniform"),
      };
      device.lost.then((info) => {
        console.error(`[cross-runtime] device lost: ${info.message}`);
      });
      if (live) {
        console.log(`[cross-runtime] mode=${mode}`);
        setShared(created);
      }
    })().catch((error) => {
      console.error(`[cross-runtime] setup failed: ${error}`);
    });

    return () => {
      live = false;
      created?.device.destroy();
    };
  }, [mode]);

  useEffect(() => {
    if (shared === null || mode === "churn") {
      return undefined;
    }
    let live = true;
    const data = new Float32Array(4);
    let tick = 0;

    const pump = () => {
      if (!live) {
        return;
      }
      tick += 1;
      data[0] = tick;
      const { device } = shared;

      if (mode === "write" || mode === "all") {
        for (let i = 0; i < OPS_PER_TICK; i += 1) {
          data[1] = i;
          device.queue.writeBuffer(shared.jsBuffer, 0, data);
        }
      }
      if (mode === "submit" || mode === "present" || mode === "all") {
        for (let i = 0; i < OPS_PER_TICK; i += 1) {
          device.queue.submit([device.createCommandEncoder().finish()]);
        }
      }
      if (mode === "create" || mode === "all") {
        for (let i = 0; i < OPS_PER_TICK; i += 1) {
          const texture = device.createTexture({
            size: [8, 8],
            format: "rgba8unorm",
            usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.TEXTURE_BINDING,
          });
          texture.destroy();
        }
      }
      if (mode === "map" || mode === "all") {
        const staging = device.createBuffer({
          size: 256,
          usage: GPUBufferUsage.MAP_WRITE | GPUBufferUsage.COPY_SRC,
        });
        staging
          .mapAsync(GPUMapMode.WRITE)
          .then(() => {
            new Float32Array(staging.getMappedRange())[0] = tick;
            staging.unmap();
            staging.destroy();
          })
          .catch(() => {
            staging.destroy();
          });
        device.queue.onSubmittedWorkDone().catch(() => {});
      }

      jsTicks.current = tick;
      if (tick % 60 === 0) {
        setTicks(tick);
      }
      setTimeout(pump, 0);
    };

    pump();
    return () => {
      live = false;
    };
  }, [shared, mode]);

  useEffect(() => {
    if (shared === null || (mode !== "churn" && mode !== "all")) {
      return undefined;
    }
    const interval = setInterval(
      () => setChurnEpoch((value) => value + 1),
      SURFACE_CHURN_INTERVAL_MS,
    );
    return () => clearInterval(interval);
  }, [shared, mode]);

  useEffect(() => {
    if (shared === null || mode !== "destroy") {
      return undefined;
    }
    const timeout = setTimeout(() => {
      console.log(
        "[cross-runtime] destroying device while the UI runtime renders",
      );
      shared.device.destroy();
    }, DESTROY_AFTER_MS);
    return () => clearTimeout(timeout);
  }, [shared, mode]);

  useEffect(() => {
    const interval = setInterval(() => setUiFrameCount(uiFrames.value), 500);
    return () => clearInterval(interval);
  }, [uiFrames]);

  const chooseMode = useCallback((next: ReproMode) => {
    setShared(null);
    setMode(next);
  }, []);

  const showChurn = mode === "churn" || mode === "all";

  return (
    <View style={styles.container}>
      <View style={styles.instructions}>
        <Text style={styles.title}>Cross-runtime device stress</Text>
        <Text selectable style={styles.copy}>
          mode: {mode} ui: {uiFrameCount} js: {ticks}
        </Text>
        <ScrollView horizontal contentContainerStyle={styles.buttons}>
          {MODES.map((value) => (
            <Button
              key={value}
              title={value}
              onPress={() => chooseMode(value)}
            />
          ))}
        </ScrollView>
      </View>
      {shared ? (
        <View style={styles.canvasHost}>
          <RenderCanvas shared={shared} mode={mode} uiFrames={uiFrames} />
          {showChurn ? <ChurnCanvas key={churnEpoch} shared={shared} /> : null}
        </View>
      ) : (
        <View style={styles.canvas} />
      )}
    </View>
  );
};

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: "black" },
  instructions: {
    paddingHorizontal: 16,
    paddingTop: 64,
    paddingBottom: 12,
    gap: 8,
    backgroundColor: "white",
  },
  title: { fontSize: 18, fontWeight: "600" },
  copy: { fontSize: 12 },
  buttons: { gap: 8, alignItems: "center" },
  canvasHost: { flex: 1 },
  canvas: { flex: 1 },
  churn: {
    position: "absolute",
    right: 12,
    bottom: 12,
    width: 96,
    height: 96,
  },
});
