import React, { useEffect, useRef } from "react";
import { Animated, Dimensions, View } from "react-native";
import { Canvas } from "react-native-webgpu";

import { redFragWGSL, triangleVertWGSL } from "../Triangle/triangle";
import { useWebGPU } from "../components/useWebGPU";

const window = Dimensions.get("window");

export const Resize = () => {
  const width = useRef(new Animated.Value(20));
  const widthRef = useRef(20);
  useEffect(() => {
    width.current.addListener(({ value }) => {
      widthRef.current = value;
    });
  }, []);
  const { canvasRef } = useWebGPU(({ context, device, presentationFormat }) => {
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
    let currentSize = { width: 0, height: 0 };
    let renderTarget: GPUTexture | undefined;
    let renderTargetView: GPUTextureView;

    return () => {
      const canvas = { width: widthRef.current, height: window.height };
      if (
        currentSize.width !== canvas.width ||
        currentSize.height !== canvas.height
      ) {
        context.canvas.width = canvas.width;
        context.canvas.height = canvas.height;
        context.configure({
          device,
          format: presentationFormat,
          alphaMode: "premultiplied",
        });
        if (renderTarget !== undefined) {
          // Destroy the previous render target
          renderTarget.destroy();
        }

        // Setting the canvas width and height will automatically resize the textures returned
        // when calling getCurrentTexture() on the context.
        currentSize = { width: canvas.width, height: canvas.height };
        renderTarget = device.createTexture({
          size: [canvas.width, canvas.height],
          sampleCount,
          format: presentationFormat,
          usage: GPUTextureUsage.RENDER_ATTACHMENT,
        });

        renderTargetView = renderTarget.createView();
      }
      if (renderTargetView) {
        const commandEncoder = device.createCommandEncoder();
        const renderPassDescriptor: GPURenderPassDescriptor = {
          colorAttachments: [
            {
              view: renderTargetView,
              resolveTarget: context.getCurrentTexture().createView(),
              clearValue: [0.2, 0.2, 0.2, 1.0],
              loadOp: "clear",
              storeOp: "store",
            },
          ],
        };

        const passEncoder =
          commandEncoder.beginRenderPass(renderPassDescriptor);
        passEncoder.setPipeline(pipeline);
        passEncoder.draw(3);
        passEncoder.end();

        device.queue.submit([commandEncoder.finish()]);
      }
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
          toValue: 20,
          duration: 4000,
          useNativeDriver: false,
        }),
      ]),
    ).start();
  }, []);

  return (
    <View style={{ flex: 1, alignItems: "center" }}>
      <Animated.View
        style={{ width: width.current, flex: 1, backgroundColor: "red" }}
      >
        <Canvas ref={canvasRef} style={{ flex: 1 }} />
      </Animated.View>
    </View>
  );
};
