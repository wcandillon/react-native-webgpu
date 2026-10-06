import React, { useEffect, useRef, useState } from "react";
import {
  Button,
  Pressable,
  ScrollView,
  StyleSheet,
  Text,
  View,
} from "react-native";
import type {
  AndroidCanvasProps,
  CanvasPresentation,
  CanvasRef,
} from "react-native-webgpu";
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
// - non-opaque HardwareBufferView (opt-in, Android 10+): pink, overlay
//   visible. Requesting it on an older device falls back to TextureView.
//   Hardware buffers are rgba8unorm: with the format toggle on rgba16float,
//   the view is replaced at runtime by the one that presents with a copy
//   (WebGPUBlitTextureView) and looks the same. The replacement sticks until
//   another view is selected.
// - non-opaque SurfaceView: blends against the window background (black),
//   overlay visible only with zOrderOnTop off (the surface sits below it).
// - non-opaque SurfaceView + zOrderOnTop: pink, overlay hidden underneath.
// - copy (presentation="copy", also on iOS): pink, overlay visible. The UI
//   thread copies each frame onto the surface instead of the rendering thread
//   presenting it.
const OPTIONS: {
  label: string;
  android?: AndroidCanvasProps;
  presentation?: CanvasPresentation;
}[] = [
  { label: "auto" },
  { label: "hardware buffer", android: { surfaceType: "HardwareBufferView" } },
  { label: "texture", android: { surfaceType: "TextureView" } },
  { label: "surface", android: { surfaceType: "SurfaceView" } },
  {
    label: "surface on top",
    android: { surfaceType: "SurfaceView", zOrderOnTop: true },
  },
  { label: "copy", presentation: "copy" },
];

const CLEAR_COLOR: GPUColor = [0.5, 0, 0, 0.5];

export const TransparencyMode = () => {
  const ref = useRef<CanvasRef>(null);
  const { log, append } = useDiagnosticLog();
  const [option, setOption] = useState(OPTIONS[0]);
  const [opaque, setOpaque] = useState(false);
  const [mounted, setMounted] = useState(true);
  const [float16, setFloat16] = useState(false);
  // One device per mounted canvas: toggling the format reconfigures the same
  // context with the same device.
  const gpu = useRef<ReturnType<typeof initGPU> | null>(null);
  const [taps, setTaps] = useState(0);

  useEffect(() => {
    if (!mounted) {
      return;
    }
    let running = true;
    let frame = 0;
    (async () => {
      gpu.current ??= initGPU(append);
      const { device, format } = await gpu.current;
      if (!running) {
        return;
      }
      const ctx = ref.current!.getContext("webgpu")!;
      ctx.configure({
        device,
        format: float16 ? "rgba16float" : format,
        alphaMode: "premultiplied",
      });
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
  }, [append, mounted, float16]);

  useEffect(() => {
    if (!mounted) {
      gpu.current = null;
    }
  }, [mounted]);

  return (
    <View style={diagnosticStyles.container}>
      <View style={diagnosticStyles.controls}>
        <Text style={diagnosticStyles.description}>
          Half-transparent red over blue. HardwareBufferView and TextureView
          keep the yellow RN overlay visible. A SurfaceView on top blends over
          it. Options update the same mounted Canvas; use hide/show to test a
          fresh mount.
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
          <Button
            testID="toggle-format"
            title={`format: ${float16 ? "rgba16float" : "preferred"}`}
            onPress={() => setFloat16((value) => !value)}
          />
        </View>
        <Text testID="view-status" style={diagnosticStyles.description}>
          view: {option.label} opaque: {String(opaque)} format:{" "}
          {float16 ? "rgba16float" : "preferred"} overlay taps: {taps}
        </Text>
      </View>
      <View style={styles.stage}>
        {mounted && (
          <Canvas
            ref={ref}
            style={diagnosticStyles.canvas}
            opaque={opaque}
            android={option.android}
            presentation={option.presentation}
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
