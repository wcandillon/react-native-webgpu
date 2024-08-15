import React, { useCallback, useEffect, useRef } from "react";
import { StyleSheet, View } from "react-native";
import { Canvas, useDevice } from "react-native-wgpu";
import type { CanvasRef } from "react-native-wgpu";

import { redFragWGSL, triangleVertWGSL } from "./triangle";

const presentationFormat = navigator.gpu.getPreferredCanvasFormat();

export function HelloTriangleMSAA() {
  const ref = useRef<CanvasRef>(null);
  const device = useDevice();
  const demo = useCallback(() => {
    const context = ref.current!.getContext("webgpu")!;
    const { canvas } = context;
    if (!context) {
      throw new Error("No context");
    }
    context.configure({
      device,
      format: presentationFormat,
      alphaMode: "premultiplied",
    });

    const sampleCount = 4;

    const pipeline = device.createRenderPipeline({
      layout: "auto",
      vertex: {
        module: device.createShaderModule({
          code: triangleVertWGSL,
        }),
      },
      fragment: {
        module: device.createShaderModule({
          code: redFragWGSL,
        }),
        targets: [
          {
            format: presentationFormat,
          },
        ],
      },
      primitive: {
        topology: "triangle-list",
      },
      multisample: {
        count: sampleCount,
      },
    });

    const texture = device.createTexture({
      size: [canvas.width, canvas.height],
      sampleCount,
      format: presentationFormat,
      usage: GPUTextureUsage.RENDER_ATTACHMENT,
    });
    const view = texture.createView();

    function frame() {
      const commandEncoder = device.createCommandEncoder();

      const renderPassDescriptor: GPURenderPassDescriptor = {
        colorAttachments: [
          {
            view,
            resolveTarget: context.getCurrentTexture().createView(),
            clearValue: [0, 0, 0, 1],
            loadOp: "clear",
            storeOp: "discard",
          },
        ],
      };

      const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
      passEncoder.setPipeline(pipeline);
      passEncoder.draw(3);
      passEncoder.end();

      device.queue.submit([commandEncoder.finish()]);
    }

    frame();
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
