import React, { useEffect, useRef, useState } from "react";
import { Button, PixelRatio, ScrollView, Text, View } from "react-native";
import type { CanvasRef, RNCanvasContext } from "react-native-webgpu";
import { Canvas } from "react-native-webgpu";

import {
  diagnosticStyles,
  drawClearFrame,
  initGPU,
  useDiagnosticLog,
} from "./surfaceLifecycle";

// configure() copies the wgpu::SurfaceConfiguration into SurfaceInfo, but the
// viewFormats array inside it is owned by the Convertor that only lives for
// the duration of the configure() call. The stored configuration keeps a
// pointer into that freed memory. Any later reconfigure re-reads it:
// - a canvas resize (this screen's button),
// - a device rotation,
// - the offscreen/onscreen transitions on background/foreground.
// The symptom is a use-after-free: typically a Dawn validation error about a
// garbage TextureFormat in viewFormats, sometimes a clean-looking frame
// (freed memory not yet reused), a crash under ASan.
//
// The button churns the native heap first (shader module allocations) to
// poison the freed allocation, then shrinks canvas.width by one pixel so the
// next getCurrentTexture() triggers the reconfigure path.
export const ViewFormatsUseAfterFree = () => {
  const ref = useRef<CanvasRef>(null);
  const { log, append } = useDiagnosticLog();
  const [gpu, setGpu] = useState<{
    device: GPUDevice;
    context: RNCanvasContext;
  } | null>(null);

  useEffect(() => {
    let running = true;
    let frame = 0;
    (async () => {
      const { device, format } = await initGPU(append);
      const ctx = ref.current!.getContext("webgpu")!;
      const canvas = ctx.canvas as HTMLCanvasElement;
      canvas.width = canvas.clientWidth * PixelRatio.get();
      canvas.height = canvas.clientHeight * PixelRatio.get();
      const viewFormat: GPUTextureFormat = `${format}-srgb` as GPUTextureFormat;
      ctx.configure({
        device,
        format,
        alphaMode: "opaque",
        viewFormats: [viewFormat],
      });
      append(`configured ${format} with viewFormats: [${viewFormat}]`);
      setGpu({ device, context: ctx });
      const loop = () => {
        if (!running) {
          return;
        }
        try {
          drawClearFrame(device, ctx, frame++);
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

  const resize = () => {
    if (!gpu) {
      return;
    }
    const { device, context } = gpu;
    // Churn the native heap so the freed viewFormats allocation gets reused
    // before the reconfigure reads it.
    for (let i = 0; i < 32; i++) {
      device.createShaderModule({
        code: `/* ${"poison".repeat(256 + i)} */
@vertex fn v() -> @builtin(position) vec4f { return vec4f(0); }`,
      });
    }
    const canvas = context.canvas as HTMLCanvasElement;
    canvas.width -= 1;
    append(
      `canvas.width -> ${canvas.width}, next frame reconfigures with the dangling viewFormats pointer`,
    );
  };

  return (
    <View style={diagnosticStyles.container}>
      <View style={diagnosticStyles.controls}>
        <Text style={diagnosticStyles.description}>
          The canvas is configured with viewFormats. Resizing forces a native
          reconfigure that re-reads the viewFormats array configure() was given;
          on a broken build that memory was freed when configure() returned.
        </Text>
        <Button
          title="Resize canvas (reconfigure with dangling viewFormats)"
          onPress={resize}
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
