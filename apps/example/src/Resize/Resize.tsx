import { useEffect } from "react";
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

// eslint-disable-next-line @typescript-eslint/no-explicit-any
const AnimatedView = Animated.View as any;

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
    let renderTarget: GPUTexture | undefined;
    let renderTargetView: GPUTextureView;
    return () => {
      const currentWidth = canvas.clientWidth * PixelRatio.get();
      const currentHeight = canvas.clientHeight * PixelRatio.get();

      // The canvas size is animating via CSS.
      // When the size changes, we need to reallocate the render target.
      // We also need to set the physical size of the canvas to match the computed CSS size.
      if (
        (currentWidth !== canvas.width ||
          currentHeight !== canvas.height ||
          !renderTargetView) &&
        currentWidth &&
        currentHeight
      ) {
        if (renderTarget !== undefined) {
          // Destroy the previous render target
          renderTarget.destroy();
        }

        // Setting the canvas width and height will automatically resize the textures returned
        // when calling getCurrentTexture() on the context.
        canvas.width = currentWidth;
        canvas.height = currentHeight;

        // Resize the multisampled render target to match the new canvas size.
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
              clearValue: [0.5, 0.5, 0.5, 0.5],
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
      <AnimatedView style={style}>
        <Canvas ref={ref} style={{ flex: 1 }} transparent />
      </AnimatedView>
    </View>
  );
};
