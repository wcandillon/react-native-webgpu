import React, { useEffect, useRef } from "react";
import { StyleSheet, View, PixelRatio, Text } from "react-native";
import Animated, {
  useAnimatedStyle,
  useSharedValue,
} from "react-native-reanimated";
import type { CanvasRef } from "react-native-wgpu";
import { Canvas } from "react-native-wgpu";

import { redFragWGSL, triangleVertWGSL } from "../Triangle/triangle";

export function ViewTransform() {
  const ref = useRef<CanvasRef>(null);

  const rotation = useSharedValue(0);
  const id = useRef<ReturnType<typeof setInterval> | null>(null);

  useEffect(() => {
    const _id = setInterval(() => {
      rotation.value += 0.05 % (Math.PI * 2);
    }, 30);
    id.current = _id;

    return () => {
      if (id.current) {
        clearInterval(id.current);
      }
    };
  }, [rotation]);

  const animatedStyle = useAnimatedStyle(() => {
    const angle = rotation.value * 10;
    return {
      transform: [{ perspective: 1000 }, { rotateY: `${angle}deg` }],
    };
  });

  useEffect(() => {
    (async () => {
      const adapter = await navigator.gpu.requestAdapter();
      if (!adapter) {
        throw new Error("No adapter");
      }

      const device = await adapter.requestDevice();
      const presentationFormat = navigator.gpu.getPreferredCanvasFormat();

      const context = ref.current!.getContext("webgpu")!;
      const canvas = context.canvas as HTMLCanvasElement;
      canvas.width = canvas.clientWidth * PixelRatio.get();
      canvas.height = canvas.clientHeight * PixelRatio.get();

      if (!context) {
        throw new Error("No context");
      }

      context.configure({
        device,
        format: presentationFormat,
        alphaMode: "premultiplied",
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
            clearValue: [0, 0, 0, 0],
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
    })();
  }, [ref]);

  return (
    <View style={styles.container}>
      {/* @ts-expect-error React 19 + Reanimated typing issue */}
      <Animated.View style={[styles.canvasContainer, animatedStyle]}>
        <Canvas ref={ref} style={styles.canvas} transparent />
        <Text style={styles.text}>AAAAAAAAAAAAAAAABBBBBBBBBBBBB</Text>
      </Animated.View>
      <View style={styles.redBackground} />
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    alignItems: "center",
    justifyContent: "center",
    backgroundColor: "#2c2c2cCC",
  },
  canvasContainer: {
    width: 300,
    height: 300,
  },
  canvas: {
    width: 300,
    height: 300,
  },
  redBackground: {
    position: "absolute",
    width: 300,
    height: 300,
    backgroundColor: "#ff0000cc",
  },
  text: {
    color: "#FFFFFF",
    fontSize: 20,
  },
});
