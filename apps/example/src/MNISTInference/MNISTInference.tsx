import React from "react";
import { Dimensions, StyleSheet, View } from "react-native";
import type { SkImage } from "@shopify/react-native-skia";
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

const surface = Skia.Surface.MakeOffscreen(SIZE, SIZE)!;
const path = Skia.Path.Make();
path.moveTo(0, 0);
path.lineTo(SIZE, SIZE);
path.close();
const paint = Skia.Paint();
paint.setColor(Skia.Color("black"));
paint.setStyle(PaintStyle.Stroke);
paint.setStrokeWidth(1);
surface.getCanvas().drawPath(path, paint);
surface.flush();
const img = surface.makeImageSnapshot().makeNonTextureImage();

export function MNISTInference() {
  const image = useSharedValue<SkImage | null>(img);
  const gesture = Gesture.Pan().onChange((e) => {});
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
