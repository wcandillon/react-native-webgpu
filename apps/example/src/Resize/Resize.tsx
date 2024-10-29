import React, { useEffect, useRef } from "react";
import { Animated, Dimensions, PixelRatio, View } from "react-native";
import { Canvas } from "react-native-wgpu";

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
  const ref = useWebGPU(({ context, device, presentationFormat, canvas }) => {
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
      if (
        currentSize.width !== canvas.clientWidth ||
        currentSize.height !== canvas.clientHeight
      ) {
        if (renderTarget !== undefined) {
          // Destroy the previous render target
          renderTarget.destroy();
        }

        // Setting the canvas width and height will automatically resize the textures returned
        // when calling getCurrentTexture() on the context.
        canvas.width = canvas.clientWidth * PixelRatio.get();
        canvas.height = canvas.clientHeight * PixelRatio.get();

        currentSize = {
          width: canvas.clientWidth,
          height: canvas.clientHeight,
        };
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
              clearValue: [0.5, 0.5, 0.5, 1],
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
        <Canvas ref={ref} style={{ flex: 1 }} />
      </Animated.View>
    </View>
  );
};
