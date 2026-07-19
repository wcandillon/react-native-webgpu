import React, { useRef } from "react";
import { Button, ScrollView, Text, View } from "react-native";
import type { CanvasRef } from "react-native-webgpu";
import { Canvas } from "react-native-webgpu";

import {
  diagnosticStyles,
  drawClearFrame,
  initGPU,
  useDiagnosticLog,
} from "./surfaceLifecycle";

// Repros for GPUCanvasContext entry points the WebGPU spec expects to fail
// gracefully but that crash or silently misbehave in the native
// implementation:
//
// 1. getCurrentTexture() before configure(): SurfaceInfo has no device yet.
//    The native size-changed path reconfigures with a null wgpu::Device
//    (CreateTexture on the offscreen path, Surface::Configure on-screen) and
//    crashes the app instead of throwing a catchable JS error.
//
// 2. configure() on a 0x0 canvas: Dawn refuses zero-sized textures, so
//    getCurrentTexture() wraps a null texture and createView() dereferences
//    it. A canvas can legitimately be measured at 0x0 for a frame (collapsed
//    layout, display: none equivalents), so this is reachable from ordinary
//    app code.
//
// 3. unconfigure(): the native method is an empty stub. After unconfigure()
//    the context keeps handing out textures as if still configured, where the
//    spec says the canvas should behave as if it was never configured.
//
// Each button is an independent repro; on a broken build the first two
// terminate the app, so relaunch between attempts.
export const ContextEdgeCases = () => {
  const ref = useRef<CanvasRef>(null);
  const { log, append } = useDiagnosticLog();

  const getCurrentTextureUnconfigured = () => {
    try {
      const ctx = ref.current!.getContext("webgpu")!;
      append("getCurrentTexture() before configure()...");
      const texture = ctx.getCurrentTexture();
      append(
        `returned a ${texture.width}x${texture.height} texture (spec: should throw)`,
      );
    } catch (e) {
      append(`threw (correct per spec): ${e}`);
    }
  };

  const zeroSizedCanvas = async () => {
    try {
      const { device, format } = await initGPU(append);
      const ctx = ref.current!.getContext("webgpu")!;
      const canvas = ctx.canvas as HTMLCanvasElement;
      canvas.width = 0;
      canvas.height = 0;
      append("configure() with canvas.width = canvas.height = 0...");
      ctx.configure({ device, format, alphaMode: "opaque" });
      const texture = ctx.getCurrentTexture();
      append(`getCurrentTexture() -> ${texture.width}x${texture.height}`);
      texture.createView();
      append("createView() survived");
    } catch (e) {
      append(`threw: ${e}`);
    }
  };

  const unconfigureStub = async () => {
    try {
      const { device, format } = await initGPU(append);
      const ctx = ref.current!.getContext("webgpu")!;
      ctx.configure({ device, format, alphaMode: "opaque" });
      drawClearFrame(device, ctx, 0);
      append("rendered one frame, calling unconfigure()...");
      ctx.unconfigure();
      const texture = ctx.getCurrentTexture();
      append(
        `getCurrentTexture() after unconfigure() -> ${texture.width}x${texture.height} texture (spec: context should be unconfigured)`,
      );
    } catch (e) {
      append(`threw (correct per spec): ${e}`);
    }
  };

  return (
    <View style={diagnosticStyles.container}>
      <View style={diagnosticStyles.controls}>
        <Text style={diagnosticStyles.description}>
          Each button exercises a context entry point that must fail gracefully
          per the WebGPU spec. On a broken build the first two crash the app.
        </Text>
        <Button
          title="getCurrentTexture() before configure()"
          onPress={getCurrentTextureUnconfigured}
        />
        <Button title="configure() a 0x0 canvas" onPress={zeroSizedCanvas} />
        <Button
          title="unconfigure() then getCurrentTexture()"
          onPress={unconfigureStub}
        />
      </View>
      <Canvas ref={ref} style={diagnosticStyles.canvas} />
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
