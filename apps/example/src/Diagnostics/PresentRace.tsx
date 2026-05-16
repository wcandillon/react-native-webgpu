import React, { useEffect, useRef } from "react";
import { StyleSheet, Text, View } from "react-native";
import type { CanvasRef } from "react-native-wgpu";
import { Canvas } from "react-native-wgpu";

type Mode = "sync" | "microtask" | "longAwait";

const runAnimatedClear = (
  device: GPUDevice,
  context: GPUCanvasContext,
  format: GPUTextureFormat,
  mode: Mode,
  shouldStop: () => boolean,
) => {
  context.configure({
    device,
    format,
    alphaMode: "premultiplied",
  });

  const frame = async () => {
    if (shouldStop()) {
      return;
    }

    const texture = context.getCurrentTexture();

    if (mode === "microtask") {
      await Promise.resolve();
    } else if (mode === "longAwait") {
      // Sleeps past one vsync interval, so the display-link tick presents
      // the surface before our submit lands.
      await new Promise((resolve) => setTimeout(resolve, 30));
    }

    const time = Date.now() / 1000;
    const r = (Math.sin(time * 2.0) + 1) / 2;
    const g = (Math.sin(time * 1.5 + Math.PI / 3) + 1) / 2;
    const b = (Math.sin(time * 1.0 + Math.PI / 2) + 1) / 2;

    const commandEncoder = device.createCommandEncoder();
    const passEncoder = commandEncoder.beginRenderPass({
      colorAttachments: [
        {
          view: texture.createView(),
          clearValue: [r, g, b, 1],
          loadOp: "clear",
          storeOp: "store",
        },
      ],
    });
    passEncoder.end();
    device.queue.submit([commandEncoder.finish()]);

    requestAnimationFrame(() => {
      frame();
    });
  };

  frame();
};

const Panel = ({ mode, label }: { mode: Mode; label: string }) => {
  const ref = useRef<CanvasRef>(null);
  useEffect(() => {
    let stopped = false;
    (async () => {
      const adapter = await navigator.gpu.requestAdapter();
      if (!adapter) {
        return;
      }
      const device = await adapter.requestDevice();
      const context = ref.current?.getContext("webgpu");
      if (!context) {
        return;
      }
      const format = navigator.gpu.getPreferredCanvasFormat();
      runAnimatedClear(device, context, format, mode, () => stopped);
    })();
    return () => {
      stopped = true;
    };
  }, [mode]);
  return (
    <View style={styles.panel}>
      <Text style={styles.label}>{label}</Text>
      <Canvas ref={ref} style={styles.canvas} />
    </View>
  );
};

export const PresentRace = () => {
  return (
    <View style={styles.container}>
      <Text style={styles.intro}>
        All three panels animate a clear color via requestAnimationFrame. The
        present is driven by a native display link, so a short microtask await
        between acquire and submit is safe. A long await (greater than one
        vsync interval) still races: the display link presents the surface
        before submit lands, producing a stale frame.
      </Text>
      <Panel mode="sync" label="Synchronous (expected: smooth rainbow)" />
      <Panel
        mode="microtask"
        label="await Promise.resolve() (expected: smooth rainbow)"
      />
      <Panel
        mode="longAwait"
        label="await setTimeout(30ms) (expected: stale/broken)"
      />
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: "#111",
    padding: 12,
  },
  intro: {
    color: "#f5f5f5",
    fontSize: 13,
    lineHeight: 18,
    marginBottom: 12,
  },
  panel: {
    flex: 1,
    marginBottom: 12,
  },
  label: {
    color: "#f5f5f5",
    fontSize: 13,
    marginBottom: 6,
  },
  canvas: {
    flex: 1,
  },
});
