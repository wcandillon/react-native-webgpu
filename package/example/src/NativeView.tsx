import React, { useEffect, useRef } from "react";
import { Button, StyleSheet, View } from "react-native";
import { gpu, WebGPUView, WebGPUViewRef } from "react-native-webgpu";
import { redFragWGSL, triangleVertWGSL } from "./components/triangle";
import { set } from "lodash";

export const NativeView = () => {
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
      alphaMode: 'premultiplied',
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

    // const texture = device.createTexture({
    //   size: [200, 200],
    //   format: "rgba8unorm",
    //   usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.COPY_SRC,
    // });
    // const textureView = texture.createView();
    const textureView = context.getCurrentTexture().createView();
    
    const renderPassDescriptor: GPURenderPassDescriptor = {
      colorAttachments: [
        {
          view: textureView,
          clearValue: [0, 1, 0, 0.5],
          loadOp: 'clear',
          storeOp: 'store',
        },
      ],
    };

    const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
    passEncoder.setPipeline(pipeline);
    passEncoder.draw(3);
    passEncoder.end();

    const tmp = commandEncoder.finish()
    device.queue.submit([tmp]);

    console.log(tmp)
  }

  useEffect(() => {
  }, [ref]);

  return <View style={style.container}>
    <Button title="Run" onPress={demo} />
    <WebGPUView ref={ref} style={style.webgpu} />
  </View>;
};

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