import React, { useEffect, useRef } from "react";
import { StyleSheet, View } from "react-native";
import type { CanvasRef } from "react-native-webgpu";
import { Canvas } from "react-native-webgpu";

import { redFragWGSL, triangleVertWGSL } from "./triangle";

function bigIntToHex(value: bigint): string {
  if (value < 0n) {
    throw new Error("Input must be a non-negative BigInt");
  }

  if (value === 0n) {
    return "0x0";
  }

  const hex = value.toString(16);
  return "0x" + hex;
}

export function HelloTriangle() {
  const ref = useRef<CanvasRef>(null);

  async function demo() {
    const adapter = await navigator.gpu.requestAdapter();
    if (!adapter) {
      throw new Error("No adapter");
    }
    const device = await adapter.requestDevice();
    const presentationFormat = navigator.gpu.getPreferredCanvasFormat();
    if (!ref.current) {
      throw new Error("No canvas reference available");
    }

    let hexResult = bigIntToHex(ref.current.getNativeSurface().surface);
    console.log(hexResult);

    const context = ref.current!.getContext("webgpu")!;

    if (!context) {
      throw new Error("No context");
    }
    context.configure({
      device,
      format: presentationFormat,
      alphaMode: "opaque",
    });

    hexResult = bigIntToHex(ref.current.getNativeSurface().surface);
    console.log(hexResult);
    console.log("Configured!");

    // const pipeline = device.createRenderPipeline({
    //   layout: "auto",
    //   vertex: {
    //     module: device.createShaderModule({
    //       code: triangleVertWGSL,
    //     }),
    //     entryPoint: "main",
    //   },
    //   fragment: {
    //     module: device.createShaderModule({
    //       code: redFragWGSL,
    //     }),
    //     entryPoint: "main",
    //     targets: [
    //       {
    //         format: presentationFormat,
    //       },
    //     ],
    //   },
    //   primitive: {
    //     topology: "triangle-list",
    //   },
    // });

    // const commandEncoder = device.createCommandEncoder();

    // const textureView = context.getCurrentTexture().createView();

    // const renderPassDescriptor: GPURenderPassDescriptor = {
    //   colorAttachments: [
    //     {
    //       view: textureView,
    //       clearValue: [0, 0, 0, 1],
    //       loadOp: "clear",
    //       storeOp: "store",
    //     },
    //   ],
    // };

    // const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
    // passEncoder.setPipeline(pipeline);
    // passEncoder.draw(3);
    // passEncoder.end();

    // device.queue.submit([commandEncoder.finish()]);

    // context.present();
  }

  useEffect(() => {
    demo();
  }, [ref]);

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
