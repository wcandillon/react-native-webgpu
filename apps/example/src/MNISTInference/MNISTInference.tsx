import React from "react";
import { Dimensions, StyleSheet, View } from "react-native";
import type { SkImage, SkSurface } from "@shopify/react-native-skia";
import {
  Canvas,
  Fill,
  Skia,
  Image,
  PaintStyle,
} from "@shopify/react-native-skia";
import { Gesture, GestureDetector } from "react-native-gesture-handler";
import { useSharedValue } from "react-native-reanimated";

import { SIZE } from "./Lib";

const { width } = Dimensions.get("window");

const paint = Skia.Paint();
paint.setColor(Skia.Color("black"));
paint.setStyle(PaintStyle.Stroke);
paint.setStrokeWidth(1);

const f = (1 / width) * SIZE;

export function MNISTInference() {
  const surface = useSharedValue<SkSurface | null>(null);
  const path = useSharedValue(Skia.Path.Make());
  const image = useSharedValue<SkImage | null>(null);
  const gesture = Gesture.Pan()
    .onStart((e) => {
      path.value.moveTo(e.x * f, e.y * f);
    })
    .onChange((e) => {
      path.value.lineTo(e.x * f, e.y * f);
      if (surface.value === null) {
        surface.value = Skia.Surface.MakeOffscreen(SIZE, SIZE)!;
      }
      surface.value.getCanvas().drawPath(path.value, paint);
      surface.value.flush();
      image.value = surface.value.makeImageSnapshot();
    });
  return (
    <View style={style.container}>
      <GestureDetector gesture={gesture}>
        <Canvas style={style.canvas}>
          <Image
            image={image}
            x={0}
            y={0}
            width={width}
            height={width}
            fit="cover"
          />
        </Canvas>
      </GestureDetector>
    </View>
  );
}

const style = StyleSheet.create({
  container: {
    flex: 1,
  },
  canvas: {
    width,
    height: width,
  },
});
