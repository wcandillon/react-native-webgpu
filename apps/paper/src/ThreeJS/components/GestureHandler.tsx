import React from "react";
import { Platform, View, StyleSheet, Dimensions } from "react-native";
import { Gesture, GestureDetector } from "react-native-gesture-handler";
import type { SharedValue } from "react-native-reanimated";
import Animated, {
  useAnimatedStyle,
  useSharedValue,
} from "react-native-reanimated";

const { width, height } = Dimensions.get("window");

import {
  convertToAffineMatrix,
  convertToColumnMajor,
  Matrix4,
  multiply4,
  rotateX,
  rotateY,
  scale,
} from "./Matrix4";

interface GestureHandlerProps {
  matrix: SharedValue<Matrix4>;
}

export const GestureHandler = ({ matrix }: GestureHandlerProps) => {
  const origin = { x: 0, y: 0, z: 0 };
  const offset = useSharedValue(Matrix4());
  const rotationSensitivity = 0.005; // Adjust as needed

  const pan = Gesture.Pan()
    .onBegin((e) => {
      offset.value = matrix.value;
      // Optionally set origin.value if needed
    })
    .onChange((e) => {
      const rotY = rotateY(e.translationX * rotationSensitivity, origin);
      const rotX = rotateX(e.translationY * rotationSensitivity, origin);
      const rot = multiply4(rotY, rotX);
      matrix.value = multiply4(offset.value, rot);
    });

  const pinch = Gesture.Pinch()
    .onBegin((e) => {
      origin.value = { x: e.focalX, y: e.focalY, z: 0 };
      offset.value = matrix.value;
    })
    .onChange((e) => {
      matrix.value = multiply4(
        offset.value,
        scale(e.scale, e.scale, 1, origin.value),
      );
    });

  const gesture = Gesture.Race(pinch, pan);
  return (
    <View style={StyleSheet.absoluteFill}>
      <GestureDetector gesture={gesture}>
        <Animated.View style={StyleSheet.absoluteFill} />
      </GestureDetector>
    </View>
  );
};
