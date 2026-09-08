import React, { useEffect, useRef, useState } from "react";
import {
  Button,
  Pressable,
  ScrollView,
  StyleSheet,
  Text,
  View,
} from "react-native";
import type { AndroidCanvasProps, CanvasRef } from "react-native-webgpu";
import { Canvas } from "react-native-webgpu";

import {
  diagnosticStyles,
  drawClearFrame,
  initGPU,
  useDiagnosticLog,
} from "./surfaceLifecycle";

// Exercises every Android backing-view combination on one mounted Canvas:
// opaque toggles in place, surfaceType/zOrderOnTop replace the child view and
// blit the last frame across. Half-transparent red is cleared over a blue
// stage with a yellow RN overlay on top:
// - opaque: solid red, overlay visible.
// - non-opaque TextureView (default): pink, overlay visible.
// - non-opaque SurfaceView: blends against the window background (black),
//   overlay visible only with zOrderOnTop off (the surface sits below it).
// - non-opaque SurfaceView + zOrderOnTop: pink, overlay hidden underneath.
const OPTIONS: { label: string; android?: AndroidCanvasProps }[] = [
  { label: "auto" },
  { label: "texture", android: { surfaceType: "TextureView" } },
  { label: "surface", android: { surfaceType: "SurfaceView" } },
  {
    label: "surface on top",
    android: { surfaceType: "SurfaceView", zOrderOnTop: true },
  },
];

const CLEAR_COLOR: GPUColor = [0.5, 0, 0, 0.5];

export const TransparencyMode = () => {
  const ref = useRef<CanvasRef>(null);
  const { log, append } = useDiagnosticLog();
  const [option, setOption] = useState(OPTIONS[0]);
  const [opaque, setOpaque] = useState(false);
  const [mounted, setMounted] = useState(true);
  const [taps, setTaps] = useState(0);

  useEffect(() => {
    if (!mounted) {
      return;
    }
    let running = true;
    let frame = 0;
    (async () => {
      const { device, format } = await initGPU(append);
      const ctx = ref.current!.getContext("webgpu")!;
      ctx.configure({ device, format, alphaMode: "premultiplied" });
      const tick = () => {
        if (!running) {
          return;
        }
        try {
          drawClearFrame(device, ctx, frame++, CLEAR_COLOR);
        } catch (e) {
          append(`frame threw: ${(e as Error).message}`);
          running = false;
          return;
        }
        setTimeout(tick, 200);
      };
      tick();
    })();
    return () => {
      running = false;
    };
  }, [append, mounted]);

  return (
    <View style={diagnosticStyles.container}>
      <View style={diagnosticStyles.controls}>
        <Text style={diagnosticStyles.description}>
          Half-transparent red over blue. TextureView keeps the yellow RN
          overlay visible. A SurfaceView on top blends over it. Options update
          the same mounted Canvas; use hide/show to test a fresh mount.
        </Text>
        <View style={styles.buttons}>
          {OPTIONS.map((value) => (
            <Button
              key={value.label}
              testID={`view-${value.label.replace(/ /g, "-")}`}
              title={value.label}
              onPress={() => setOption(value)}
            />
          ))}
        </View>
        <View style={styles.buttons}>
          <Button
            testID="toggle-opaque"
            title={`opaque: ${opaque}`}
            onPress={() => setOpaque((value) => !value)}
          />
          <Button
            testID="toggle-canvas"
            title={mounted ? "hide canvas" : "show canvas"}
            onPress={() => setMounted((value) => !value)}
          />
        </View>
        <Text testID="view-status" style={diagnosticStyles.description}>
          view: {option.label} opaque: {String(opaque)} overlay taps: {taps}
        </Text>
      </View>
      <View style={styles.stage}>
        {mounted && (
          <Canvas
            ref={ref}
            style={diagnosticStyles.canvas}
            opaque={opaque}
            android={option.android}
          />
        )}
        <Pressable
          testID="canvas-overlay"
          style={styles.overlay}
          onPress={() => setTaps((t) => t + 1)}
        >
          <Text>overlay (tap me)</Text>
        </Pressable>
      </View>
      <ScrollView style={diagnosticStyles.log}>
        {log.map((line, i) => (
          <Text key={i} style={diagnosticStyles.logLine}>
            {line}
          </Text>
        ))}
      </ScrollView>
    </View>
  );
};

const styles = StyleSheet.create({
  buttons: { flexDirection: "row", flexWrap: "wrap", gap: 8 },
  stage: { flex: 1, backgroundColor: "blue" },
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
