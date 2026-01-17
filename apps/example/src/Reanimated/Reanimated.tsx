import React, { useEffect, useRef } from "react";
import { StyleSheet, View } from "react-native";
import type { CanvasRef, RNCanvasContext } from "react-native-wgpu";
import { Canvas } from "react-native-wgpu";
import type { SharedValue } from "react-native-reanimated";
import { runOnUI, useSharedValue } from "react-native-reanimated";

import { redFragWGSL, triangleVertWGSL } from "../Triangle/triangle";

const webGPUDemo = (
  runAnimation: SharedValue<boolean>,
  device: GPUDevice,
  context: RNCanvasContext,
  presentationFormat: GPUTextureFormat,
) => {
  "worklet";
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
  const frame = () => {
    console.log(Date.now());
    const commandEncoder = device.createCommandEncoder();

    const textureView = context.getCurrentTexture().createView();

    // Animate the clearValue color based on Date.now()
    const time = Date.now() / 1000; // Convert to seconds for smoother animation

    // Create animated RGB values using sine waves with different frequencies
    const r = (Math.sin(time * 2) + 1) / 2; // Red channel oscillates faster
    const g = (Math.sin(time * 1.5 + Math.PI / 3) + 1) / 2; // Green with phase offset
    const b = (Math.sin(time * 1 + Math.PI / 2) + 1) / 2; // Blue with different phase

    const renderPassDescriptor: GPURenderPassDescriptor = {
      colorAttachments: [
        {
          view: textureView,
          clearValue: [r, g, b, 1],
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
    if (runAnimation.value) {
      requestAnimationFrame(frame);
    }
  };
  frame();
};

export function Reanimated() {
  const runAnimation = useSharedValue(true);
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
      const presentationFormat = navigator.gpu.getPreferredCanvasFormat();
      // TODO: stop the animation on unmount
      runOnUI(webGPUDemo)(runAnimation, device, ctx, presentationFormat);
    };
    initWebGPU();
    return () => {
      runAnimation.value = false;
    };
  });
  return (
    <View style={style.container}>
      <Canvas ref={ref} style={style.webgpu} />
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
