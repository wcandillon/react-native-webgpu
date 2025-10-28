import React, { useEffect, useRef } from "react";
import { StyleSheet, View } from "react-native";
import type { CanvasRef } from "react-native-wgpu";
import { Canvas } from "react-native-wgpu";
import { runOnUI } from "react-native-reanimated";

import { redFragWGSL, triangleVertWGSL } from "../Triangle/triangle";

const webGPUDemo = (device: GPUDevice, context: GPUCanvasContext) => {
  "worklet";
  const presentationFormat = navigator.gpu.getPreferredCanvasFormat();

  if (!context) {
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
};

export function Reanimated() {
  const ref = useRef<CanvasRef>(null);
  useEffect(() => {
    const initWebGPU = async () => {
      const adapter = await navigator.gpu.requestAdapter();
      if (!adapter) {
        console.error("Failed to get GPU adapter");
        return;
      }
      const device = await adapter.requestDevice();
      if (!device) {
        console.error("Failed to get GPU device");
        return;
      }
      const ctx = ref.current!.getContext("webgpu");
      if (!ctx) {
        console.error("Failed to get GPU canvas context");
        return;
      }
      // TODO: stop the animation on unmount
      runOnUI(webGPUDemo)(device, ctx);
    };
    initWebGPU();
  });
  return (
    <View style={style.container}>
      <Canvas ref={ref} style={style.webgpu} transparent />
    </View>
  );
}

const style = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: "rgb(90, 180, 255)",
  },
  webgpu: {
    flex: 1,
  },
});
