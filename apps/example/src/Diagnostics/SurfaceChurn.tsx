import React, { useEffect, useRef, useState } from "react";
import { Button, ScrollView, Text, View } from "react-native";
import type { CanvasRef } from "react-native-webgpu";
import { Canvas } from "react-native-webgpu";

import {
  diagnosticStyles,
  drawClearFrame,
  initGPU,
  useDiagnosticLog,
} from "./surfaceLifecycle";

// Automated stress for the attach/detach transitions. While a frame loop
// renders on the JS thread, the canvas is fully remounted every 400ms (every
// third remount also flips the transparent prop, which swaps the Android view
// flavor). Each epoch tears the previous native view down on the platform
// thread while the previous frame may still be in flight, exercising:
// - native teardown (MetalView dealloc / TextureView destroy) racing
//   getCurrentTexture()/present() on the JS thread,
// - registry add/remove churn against context creation,
// - repeated offscreen/onscreen transitions and reconfigures.
//
// Expected: the gradient flickers but the app survives indefinitely with no
// validation errors. On a broken build this crashes within seconds
// (use-after-free of the native layer/window) or degrades into a black canvas
// and a stream of validation errors.
export const SurfaceChurn = () => {
  const { log, append } = useDiagnosticLog();
  const [running, setRunning] = useState(false);
  const [epoch, setEpoch] = useState(0);
  const ref = useRef<CanvasRef>(null);
  const deviceRef = useRef<{
    device: GPUDevice;
    format: GPUTextureFormat;
  } | null>(null);

  useEffect(() => {
    if (!running) {
      return undefined;
    }
    const interval = setInterval(() => {
      setEpoch((e) => e + 1);
    }, 400);
    return () => clearInterval(interval);
  }, [running]);

  useEffect(() => {
    if (!running) {
      return undefined;
    }
    let live = true;
    let frame = 0;
    (async () => {
      if (deviceRef.current === null) {
        deviceRef.current = await initGPU(append);
      }
      const { device, format } = deviceRef.current;
      if (!live || !ref.current) {
        return;
      }
      const ctx = ref.current.getContext("webgpu")!;
      ctx.configure({ device, format, alphaMode: "premultiplied" });
      if (epoch % 10 === 0) {
        append(`epoch ${epoch}`);
      }
      const loop = () => {
        if (!live) {
          return;
        }
        try {
          drawClearFrame(device, ctx, epoch * 24 + frame++);
        } catch (e) {
          append(`epoch ${epoch} frame ${frame}: ${e}`);
        }
        requestAnimationFrame(loop);
      };
      loop();
    })();
    return () => {
      live = false;
    };
  }, [running, epoch, append]);

  return (
    <View style={diagnosticStyles.container}>
      <View style={diagnosticStyles.controls}>
        <Text style={diagnosticStyles.description}>
          Remounts the canvas every 400ms while rendering every frame, flipping
          the transparent flag every third remount. Expected: flicker, but no
          crash and no validation errors. Leave it running for a minute.
        </Text>
        <Button
          title={running ? "Stop churn" : "Start churn"}
          onPress={() => setRunning((r) => !r)}
        />
      </View>
      {running ? (
        <Canvas
          key={epoch}
          ref={ref}
          style={diagnosticStyles.canvas}
          transparent={Math.floor(epoch / 3) % 2 === 0}
        />
      ) : (
        <View style={diagnosticStyles.canvas} />
      )}
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
