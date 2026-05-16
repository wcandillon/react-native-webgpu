import React, { useEffect, useRef } from "react";
import { StyleSheet, Text, View } from "react-native";
import type { CanvasRef } from "react-native-wgpu";
import { Canvas } from "react-native-wgpu";

type Mode = "combined" | "split";

const runPair = (
  device: GPUDevice,
  contextA: GPUCanvasContext,
  contextB: GPUCanvasContext,
  format: GPUTextureFormat,
  mode: Mode,
  shouldStop: () => boolean,
) => {
  contextA.configure({ device, format, alphaMode: "premultiplied" });
  contextB.configure({ device, format, alphaMode: "premultiplied" });

  const frame = () => {
    if (shouldStop()) {
      return;
    }

    const textureA = contextA.getCurrentTexture();
    const textureB = contextB.getCurrentTexture();

    const time = Date.now() / 1000;
    const r = (Math.sin(time * 2.0) + 1) / 2;
    const g = (Math.sin(time * 1.5 + Math.PI / 3) + 1) / 2;
    const b = (Math.sin(time * 1.0 + Math.PI / 2) + 1) / 2;

    const drawClear = (
      encoder: GPUCommandEncoder,
      view: GPUTextureView,
      color: GPUColor,
    ) => {
      const pass = encoder.beginRenderPass({
        colorAttachments: [
          {
            view,
            clearValue: color,
            loadOp: "clear",
            storeOp: "store",
          },
        ],
      });
      pass.end();
    };

    if (mode === "combined") {
      // One encoder, two passes targeting two different surfaces, one
      // command buffer, one submit. Tracks that beginRenderPass accumulates
      // every color-attachment surface into the encoder's presentable set.
      const encoder = device.createCommandEncoder();
      drawClear(encoder, textureA.createView(), [r, g, b, 1]);
      drawClear(encoder, textureB.createView(), [1 - r, 1 - g, 1 - b, 1]);
      device.queue.submit([encoder.finish()]);
    } else {
      // Two encoders, two command buffers, one submit. Tracks that
      // queue.submit aggregates presentable surfaces across every command
      // buffer in the array.
      const encoderA = device.createCommandEncoder();
      drawClear(encoderA, textureA.createView(), [r, g, b, 1]);
      const encoderB = device.createCommandEncoder();
      drawClear(encoderB, textureB.createView(), [1 - r, 1 - g, 1 - b, 1]);
      device.queue.submit([encoderA.finish(), encoderB.finish()]);
    }

    requestAnimationFrame(frame);
  };

  frame();
};

const Pair = ({ mode, label }: { mode: Mode; label: string }) => {
  const refA = useRef<CanvasRef>(null);
  const refB = useRef<CanvasRef>(null);
  useEffect(() => {
    let stopped = false;
    (async () => {
      const adapter = await navigator.gpu.requestAdapter();
      if (!adapter) {
        return;
      }
      const device = await adapter.requestDevice();
      const contextA = refA.current?.getContext("webgpu");
      const contextB = refB.current?.getContext("webgpu");
      if (!contextA || !contextB) {
        return;
      }
      const format = navigator.gpu.getPreferredCanvasFormat();
      runPair(device, contextA, contextB, format, mode, () => stopped);
    })();
    return () => {
      stopped = true;
    };
  }, [mode]);
  return (
    <View style={styles.pair}>
      <Text style={styles.label}>{label}</Text>
      <View style={styles.row}>
        <Canvas ref={refA} style={styles.canvas} />
        <Canvas ref={refB} style={styles.canvas} />
      </View>
    </View>
  );
};

export const MultiCanvasSubmit = () => {
  return (
    <View style={styles.container}>
      <Text style={styles.intro}>
        Each row drives two canvases that render inverted hues from a single
        submit. If the presentable-surface tracking is broken, one of the two
        canvases will stop updating (no display-link tick will present it).
      </Text>
      <Pair
        mode="combined"
        label="One encoder, two passes, one command buffer"
      />
      <Pair
        mode="split"
        label="Two encoders, two command buffers, one submit"
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
  pair: {
    flex: 1,
    marginBottom: 12,
  },
  label: {
    color: "#f5f5f5",
    fontSize: 13,
    marginBottom: 6,
  },
  row: {
    flex: 1,
    flexDirection: "row",
  },
  canvas: {
    flex: 1,
    marginRight: 6,
  },
});
