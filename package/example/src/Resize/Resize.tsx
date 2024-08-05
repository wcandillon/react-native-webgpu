import React, { useEffect, useRef } from "react";
import { Animated, View } from "react-native";
import { Canvas } from "react-native-webgpu";

import { redFragWGSL, triangleVertWGSL } from "../Triangle/triangle";
import { useWebGPU } from "../components/useWebGPU";

export const Resize = () => {
  const width = useRef(new Animated.Value(0));
  const { canvasRef } = useWebGPU(({ context, device, presentationFormat }) => {
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

    return () => {
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
    };
  });

  useEffect(() => {
    Animated.loop(
      Animated.timing(width.current, {
        toValue: 500,
        duration: 4000,
        useNativeDriver: false,
      }),
    ).start();
  }, []);

  return (
    <View style={{ flex: 1, alignItems: "center" }}>
      <Animated.View style={{ flex: 1, width: width.current }}>
        <Canvas ref={canvasRef} style={{ flex: 1 }} />
      </Animated.View>
    </View>
  );
};
