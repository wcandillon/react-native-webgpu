import React, { useEffect, useRef, useState } from "react";
import { AppState, ScrollView, Switch, Text, View } from "react-native";
import type { CanvasRef } from "react-native-webgpu";
import { Canvas } from "react-native-webgpu";

import {
  diagnosticStyles,
  drawClearFrame,
  initGPU,
  useDiagnosticLog,
} from "./surfaceLifecycle";

// Backgrounding the app destroys the native surface while the JS context
// stays alive. This screen renders continuously; background the app, wait a
// second, and come back. Primarily an Android repro (iOS keeps the
// CAMetalLayer alive in the background).
//
// On a broken build the two Android view flavors fail differently:
// - transparent (TextureView): onSurfaceTextureDestroyed removes the surface
//   registry entry entirely. The resumed view registers a fresh entry the JS
//   context has never seen, so the canvas stays black forever (and the
//   context renders into the orphaned surface on a destroyed window).
// - opaque (SurfaceView): the surface detaches to offscreen and reattaches on
//   resume, but the reattach blit presents a frame the context never
//   acquired (watch for a present-without-acquire validation error in the
//   log) and permanently widens the configured usage with CopyDst.
//
// Flipping the switch while rendering tears down the current native view the
// same way backgrounding does, so it reproduces the TextureView teardown
// without leaving the app.
export const BackgroundDetach = () => {
  const ref = useRef<CanvasRef>(null);
  const { log, append } = useDiagnosticLog();
  const [transparent, setTransparent] = useState(true);

  useEffect(() => {
    const sub = AppState.addEventListener("change", (state) => {
      append(`AppState: ${state}`);
    });
    return () => sub.remove();
  }, [append]);

  useEffect(() => {
    let running = true;
    let frame = 0;
    (async () => {
      const { device, format } = await initGPU(append);
      const ctx = ref.current!.getContext("webgpu")!;
      ctx.configure({ device, format, alphaMode: "premultiplied" });
      append(
        "rendering, background the app and come back (or flip the switch)",
      );
      const loop = () => {
        if (!running) {
          return;
        }
        try {
          drawClearFrame(device, ctx, frame++);
          if (frame % 120 === 0) {
            append(`frame ${frame} ok`);
          }
        } catch (e) {
          append(`frame ${frame}: ${e}`);
        }
        requestAnimationFrame(loop);
      };
      loop();
    })();
    return () => {
      running = false;
    };
  }, [append]);

  return (
    <View style={diagnosticStyles.container}>
      <View style={diagnosticStyles.controls}>
        <Text style={diagnosticStyles.description}>
          Background the app and return: the animated gradient must resume. On a
          broken build the transparent canvas stays black forever (Android), and
          the opaque one logs a validation error on resume.
        </Text>
        <View style={styles.row}>
          <Text style={diagnosticStyles.description}>
            transparent (TextureView on Android)
          </Text>
          <Switch value={transparent} onValueChange={setTransparent} />
        </View>
      </View>
      <Canvas
        ref={ref}
        style={diagnosticStyles.canvas}
        transparent={transparent}
      />
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

const styles = {
  row: {
    flexDirection: "row" as const,
    alignItems: "center" as const,
    justifyContent: "space-between" as const,
  },
};
