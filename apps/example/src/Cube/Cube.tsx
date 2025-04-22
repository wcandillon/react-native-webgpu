import React from "react";
import { PixelRatio, StyleSheet, View } from "react-native";
import { Canvas, useCanvasEffect } from "react-native-wgpu";
import { Gesture, GestureDetector } from "react-native-gesture-handler";
import { runOnUI, useSharedValue, withDecay } from "react-native-reanimated";
import { useClock } from "@shopify/react-native-skia";

import { useMakeJsThreadBusy } from "../components/useMakeJSThreadBusy";
import { concat } from "../components/Matrix4";

import { CubeScene } from "./CubeScene";

export function Cube() {
  const rotateX = useSharedValue(0);
  const rotateY = useSharedValue(0);
  const gesture = Gesture.Pan()
    .onChange((e) => {
      rotateY.value += e.changeX * 0.01;
      rotateX.value += e.changeY * 0.01;
    })
    .onEnd(({ velocityX, velocityY }) => {
      rotateX.value = withDecay({ velocity: velocityY * 0.01 });
      rotateY.value = withDecay({ velocity: velocityX * 0.01 });
    });
  const ref = useCanvasEffect(async () => {
    const context = ref.current!.getContext("webgpu")!;
    const adapter = await navigator.gpu.requestAdapter();
    if (!adapter) {
      throw new Error("Adapter not found");
    }
    const device = await adapter.requestDevice();
    const presentationFormat = navigator.gpu.getPreferredCanvasFormat();
    const canvas = context.canvas as HTMLCanvasElement;
    canvas.width = canvas.clientWidth * PixelRatio.get();
    canvas.height = canvas.clientHeight * PixelRatio.get();
    runOnUI(() => {
      const cube = new CubeScene(device, context, presentationFormat);
      cube.init();
      function frame() {
        cube.render(rotateX, rotateY);
        requestAnimationFrame(frame);
      }
      requestAnimationFrame(frame);
    })();
  });

  return (
    <View style={style.container}>
      <Canvas ref={ref} style={style.webgpu} transparent />
      <GestureDetector gesture={gesture}>
        <View style={StyleSheet.absoluteFill} />
      </GestureDetector>
    </View>
  );
}

const style = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: "rgb(90, 180, 255)",
  },
  webgpu: {
    flex: 1,
  },
});
