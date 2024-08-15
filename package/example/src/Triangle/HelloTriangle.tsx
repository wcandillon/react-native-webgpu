import React, { useCallback, useEffect, useRef } from "react";
import { StyleSheet, View } from "react-native";
import type { CanvasRef } from "react-native-wgpu";
import { Canvas, useDevice } from "react-native-wgpu";

import { redFragWGSL, triangleVertWGSL } from "./triangle";

const presentationFormat = navigator.gpu.getPreferredCanvasFormat();

export function HelloTriangle() {
  const ref = useRef<CanvasRef>(null);
  const device = useDevice();
  const demo = useCallback(() => {
    const context = ref.current!.getContext("webgpu")!;

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
  }, []);

  useEffect(() => {
    demo();
  }, []);

  return (
    <View style={style.container}>
      <Canvas ref={ref} style={style.webgpu} />
    </View>
  );
}

const style = StyleSheet.create({
  container: {
    flex: 1,
  },
  webgpu: {
    flex: 1,
  },
});
