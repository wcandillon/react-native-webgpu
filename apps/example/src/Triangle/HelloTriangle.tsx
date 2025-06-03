import React, { useCallback } from "react";
import { StyleSheet, View, PixelRatio } from "react-native";
import { Canvas, useCanvasEffect } from "react-native-wgpu";

import { redFragWGSL, triangleVertWGSL } from "./triangle";

export function HelloTriangle() {
  const ref = useCanvasEffect(
    useCallback(async ({ signal, canvasRef }) => {
      const adapter = await navigator.gpu.requestAdapter();
      if (!adapter) {
        throw new Error("No adapter");
      }
      const device = await adapter.requestDevice();

      if (signal.aborted) {
        device.destroy();
        return;
      }

      const presentationFormat = navigator.gpu.getPreferredCanvasFormat();

      const context = canvasRef.getContext("webgpu")!;
      const canvas = context.canvas as HTMLCanvasElement;
      canvas.width = canvas.clientWidth * PixelRatio.get();
      canvas.height = canvas.clientHeight * PixelRatio.get();

      if (!context) {
        device.destroy();
        throw new Error("No context");
      }

      context.configure({
        device,
        format: presentationFormat,
        alphaMode: "premultiplied",
      });

      const pipeline = device.createRenderPipeline({
        layout: "auto",
        vertex: {
          module: device.createShaderModule({
            code: triangleVertWGSL,
          }),
          entryPoint: "main",
        },
        fragment: {
          module: device.createShaderModule({
            code: redFragWGSL,
          }),
          entryPoint: "main",
          targets: [
            {
              format: presentationFormat,
            },
          ],
        },
        primitive: {
          topology: "triangle-list",
        },
      });

      const commandEncoder = device.createCommandEncoder();

      const textureView = context.getCurrentTexture().createView();

      const renderPassDescriptor: GPURenderPassDescriptor = {
        colorAttachments: [
          {
            view: textureView,
            clearValue: [0, 0, 0, 0],
            loadOp: "clear",
            storeOp: "store",
          },
        ],
      };

      const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
      passEncoder.setPipeline(pipeline);
      passEncoder.draw(3);
      passEncoder.end();

      device.queue.submit([commandEncoder.finish()]);

      context.present();
      device.destroy();
    }, []),
  );

  return (
    <View style={style.container}>
      <View style={{ flex: 1, backgroundColor: "#3498db" }} />
      <Canvas ref={ref} style={StyleSheet.absoluteFill} transparent />
    </View>
  );
}

const style = StyleSheet.create({
  container: {
    flex: 1,
  },
});
