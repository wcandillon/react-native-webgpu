import React, { useEffect, useRef, useState } from "react";
import { Button, Pressable, StyleSheet, Text, View } from "react-native";
import type { CanvasRef } from "react-native-webgpu";
import { Canvas } from "react-native-webgpu";

type Mode = "default" | "texture" | "surface-overlay";

const ClearCanvas = ({ mode }: { mode: Mode }) => {
  const ref = useRef<CanvasRef>(null);

  useEffect(() => {
    let live = true;
    (async () => {
      const adapter = await navigator.gpu.requestAdapter();
      const device = await adapter!.requestDevice();
      const context = ref.current?.getContext("webgpu");
      if (!context || !live) {
        return;
      }
      context.configure({
        device,
        format: navigator.gpu.getPreferredCanvasFormat(),
        alphaMode: "premultiplied",
      });
      const frame = () => {
        if (!live) {
          return;
        }
        const encoder = device.createCommandEncoder();
        const pass = encoder.beginRenderPass({
          colorAttachments: [
            {
              view: context.getCurrentTexture().createView(),
              clearValue: [0.5, 0, 0, 0.5],
              loadOp: "clear",
              storeOp: "store",
            },
          ],
        });
        pass.end();
        device.queue.submit([encoder.finish()]);
        context.present();
        setTimeout(frame, 200);
      };
      frame();
    })().catch((error) => {
      console.error(`[transparency] setup failed: ${error}`);
    });
    return () => {
      live = false;
    };
  }, [mode]);

  return (
    <Canvas
      ref={ref}
      transparent
      {...(mode === "default" ? {} : { androidTransparencyMode: mode })}
      style={styles.canvas}
    />
  );
};

export const TransparencyMode = () => {
  const [mode, setMode] = useState<Mode>("default");
  const [taps, setTaps] = useState(0);

  return (
    <View style={styles.container}>
      <Text style={styles.copy}>
        Half-transparent red canvas over a blue backdrop. In &quot;texture&quot;
        mode the yellow overlay below stays visible on top of the canvas. In
        &quot;surface-overlay&quot; mode the canvas gets its own layer and
        covers the overlay.
      </Text>
      <View style={styles.buttons}>
        <Button title="default" onPress={() => setMode("default")} />
        <Button title="texture" onPress={() => setMode("texture")} />
        <Button
          title="surface-overlay"
          onPress={() => setMode("surface-overlay")}
        />
      </View>
      <Text style={styles.copy}>
        mode: {mode} overlay taps: {taps}
      </Text>
      <View style={styles.stage}>
        <ClearCanvas key={mode} mode={mode} />
        <Pressable style={styles.overlay} onPress={() => setTaps((t) => t + 1)}>
          <Text>overlay (tap me)</Text>
        </Pressable>
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  container: { flex: 1, padding: 16, gap: 8, paddingTop: 64 },
  copy: { fontSize: 13 },
  buttons: { flexDirection: "row", gap: 12 },
  stage: { flex: 1, backgroundColor: "blue" },
  canvas: { flex: 1 },
  overlay: {
    position: "absolute",
    left: 24,
    right: 24,
    bottom: 40,
    padding: 12,
    alignItems: "center",
    backgroundColor: "yellow",
  },
});
