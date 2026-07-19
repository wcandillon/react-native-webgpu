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

// The frame loop deliberately keeps calling getCurrentTexture()/present()
// after the Canvas is unmounted. This is legal from the JS side: the context
// object is still alive, and the documented behavior for a detached surface
// is to fall back to offscreen rendering.
//
// On a broken build, native view teardown frees the surface out from under
// the context:
// - iOS: MetalView dealloc removes the registry entry while the stored
//   wgpu::Surface still wraps the now-released CAMetalLayer. The next
//   getCurrentTexture() asks the dead layer for a drawable: EXC_BAD_ACCESS.
// - Android (transparent canvas / TextureView): onSurfaceTextureDestroyed
//   removes the registry entry without detaching, so the context keeps a
//   wgpu::Surface on a destroyed window: dead-window errors or a crash.
//
// The transparent prop is set so Android takes the TextureView path, which is
// the one that crashes rather than silently going black.
export const RenderAfterUnmount = () => {
  const ref = useRef<CanvasRef>(null);
  const { log, append } = useDiagnosticLog();
  const [mounted, setMounted] = useState(true);

  useEffect(() => {
    let running = true;
    let frame = 0;
    (async () => {
      const { device, format } = await initGPU(append);
      const ctx = ref.current!.getContext("webgpu")!;
      ctx.configure({ device, format, alphaMode: "premultiplied" });
      append("rendering, unmount the canvas while the loop keeps going");
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
          A frame loop renders into the canvas and keeps running after the
          canvas unmounts. Expected: the context detaches and falls back to
          offscreen rendering. On a broken build the next frame uses the freed
          native surface and crashes.
        </Text>
        <Button
          title={mounted ? "Unmount canvas" : "Remount canvas"}
          onPress={() => setMounted((m) => !m)}
        />
      </View>
      {mounted ? (
        <Canvas ref={ref} style={diagnosticStyles.canvas} transparent />
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
