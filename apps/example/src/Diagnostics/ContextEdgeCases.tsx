import React, { useRef, useState } from "react";
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
// 4. device.destroy() on the device a mounted Canvas is configured with.
//    Dawn's Vulkan backend cannot detach a swapchain after its device was
//    destroyed, so the Canvas unmount crashed on Android. Per spec the canvas
//    stays configured (getCurrentTexture() must not throw) and configure()
//    with a replacement device must render on screen again; the unmount
//    toggle then checks the swapchain teardown. "unconfigure() first" covers
//    the idiomatic cleanup order, where Dawn parks the old swapchain in the
//    surface for reuse instead of detaching it.
//
// Each button is an independent repro; on a broken build the first two
// terminate the app, so relaunch between attempts.
export const ContextEdgeCases = () => {
  const ref = useRef<CanvasRef>(null);
  const { log, append } = useDiagnosticLog();
  const [mounted, setMounted] = useState(true);

  const destroyDevice = async (unconfigureFirst: boolean) => {
    try {
      const { device, format } = await initGPU(append);
      const ctx = ref.current!.getContext("webgpu")!;
      ctx.configure({ device, format, alphaMode: "opaque" });
      drawClearFrame(device, ctx, 0);
      if (unconfigureFirst) {
        append("rendered one frame, calling unconfigure()...");
        ctx.unconfigure();
      } else {
        append("rendered one frame");
      }
      append("calling device.destroy()...");
      device.destroy();
      if (!unconfigureFirst) {
        const texture = ctx.getCurrentTexture();
        append(
          `getCurrentTexture() after destroy() -> ${texture.width}x${texture.height} (spec: invalid texture, no throw)`,
        );
        ctx.present();
      }
      const replacement = await initGPU(append);
      ctx.configure({
        device: replacement.device,
        format: replacement.format,
        alphaMode: "opaque",
      });
      drawClearFrame(replacement.device, ctx, 30);
      append(
        "reconfigured with a new device and rendered: the canvas should show a new color. Now unmount the canvas.",
      );
    } catch (e) {
      append(`threw: ${e}`);
    }
  };

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
        <Button
          title="device.destroy() then reconfigure"
          onPress={() => destroyDevice(false)}
        />
        <Button
          title="unconfigure(), device.destroy(), reconfigure"
          onPress={() => destroyDevice(true)}
        />
        <Button
          title={mounted ? "unmount canvas" : "mount canvas"}
          onPress={() => setMounted((m) => !m)}
        />
      </View>
      {mounted && <Canvas ref={ref} style={diagnosticStyles.canvas} />}
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
