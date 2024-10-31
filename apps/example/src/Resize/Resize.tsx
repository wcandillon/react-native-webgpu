import React, { useEffect } from "react";
import { Dimensions, PixelRatio, View } from "react-native";
import { Canvas } from "react-native-wgpu";
import Animated, {
  cancelAnimation,
  Easing,
  useAnimatedStyle,
  useDerivedValue,
  useSharedValue,
  withRepeat,
  withTiming,
} from "react-native-reanimated";

import { redFragWGSL, triangleVertWGSL } from "../Triangle/triangle";
import { useWebGPU } from "../components/useWebGPU";

const win = Dimensions.get("window");

export const useLoop = ({ duration }: { duration: number }) => {
  const progress = useSharedValue(0);
  useEffect(() => {
    progress.value = withRepeat(
      withTiming(1, { duration, easing: Easing.inOut(Easing.ease) }),
      -1,
      true,
    );
    return () => {
      cancelAnimation(progress);
    };
  }, [duration, progress]);
  return progress;
};

export const Resize = () => {
  const progress = useLoop({ duration: 4000 });
  const width = useDerivedValue(() => {
    return 20 + progress.value * (win.width - 20);
  });
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
        const a = 0.7;
        const renderPassDescriptor: GPURenderPassDescriptor = {
          colorAttachments: [
            {
              view: renderTargetView,
              resolveTarget: context.getCurrentTexture().createView(),
              clearValue: [a, a, a, a],
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
  const style = useAnimatedStyle(() => {
    return { width: width.value, flex: 1, backgroundColor: "cyan" };
  });
  return (
    <View style={{ flex: 1, alignItems: "center" }}>
      <Animated.View style={style}>
        <Canvas ref={ref} style={{ flex: 0.5 }} transparent />
      </Animated.View>
    </View>
  );
};
