import React, { useEffect, useRef } from "react";
import { Animated, Dimensions, View } from "react-native";
import { Canvas } from "react-native-webgpu";

import { redFragWGSL, triangleVertWGSL } from "../Triangle/triangle";
import { useWebGPU } from "../components/useWebGPU";

const window = Dimensions.get("window");

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
      Animated.sequence([
        Animated.timing(width.current, {
          toValue: window.width,
          duration: 4000,
          useNativeDriver: false,
        }),
        Animated.timing(width.current, {
          toValue: 0,
          duration: 4000,
          useNativeDriver: false,
        }),
      ]),
    ).start();
  }, []);

  return (
    <View style={{ flex: 1, alignItems: "center" }}>
      <Animated.View
        style={{ width: width.current, backgroundColor: "red", flex: 1 }}
      />
    </View>
  );
};
