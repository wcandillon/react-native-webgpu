import React from "react";
import { StyleSheet, View } from "react-native";
import { Canvas, useCanvasEffect } from "react-native-wgpu";
import { Gesture, GestureDetector } from "react-native-gesture-handler";
import { useSharedValue, withDecay } from "react-native-reanimated";
import { useClock } from "@shopify/react-native-skia";

import { CubeScene } from "./CubeScene";

export function Cube() {
  const clock = useClock();
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
    const context = ref.current?.getContext("webgpu");
    if (!context) {
      throw new Error("Context not initialized");
    }
    const scene = new CubeScene(context);
    await scene.init();
    function frame() {
      scene.render(rotateX, rotateY);
      requestAnimationFrame(frame);
    }
    requestAnimationFrame(frame);
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
