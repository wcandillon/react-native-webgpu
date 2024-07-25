import React, { useEffect, useRef } from "react";
import { StyleSheet, View } from "react-native";
import type { WebGPUViewRef } from "react-native-webgpu";
import { gpu, WebGPUView } from "react-native-webgpu";

import { redFragWGSL, triangleVertWGSL } from "./components/triangle";

export function HelloTriangle() {
  const ref = useRef<WebGPUViewRef>(null);

  async function demo() {
    const adapter = await gpu.requestAdapter();
    if (!adapter) {
      throw new Error("No adapter");
    }
    const device = await adapter.requestDevice();
    const presentationFormat = gpu.getPreferredCanvasFormat();

    const context = ref.current?.getContext("webgpu");
    if (!context) {
      throw new Error("No context");
    }

    context.configure({
      device,
      format: presentationFormat,
      alphaMode: "opaque",
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
            format: "rgba8unorm",
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
          clearValue: [0, 0, 0, 1],
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
  }

  useEffect(() => {
    demo();
  }, [ref]);

  return (
    <View style={style.container}>
      <WebGPUView ref={ref} style={style.webgpu} />
    </View>
  );
}

const style = StyleSheet.create({
  container: {
    flex: 1,
    justifyContent: "center",
    alignItems: "center",
  },
  webgpu: {
    width: 200,
    height: 200,
    justifyContent: "center",
    alignItems: "center",
  },
});
