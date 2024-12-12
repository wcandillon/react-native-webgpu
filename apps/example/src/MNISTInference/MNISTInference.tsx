import React, { useEffect } from "react";
import { Dimensions, StyleSheet, View } from "react-native";
import type { SkImage, SkSurface } from "@shopify/react-native-skia";
import {
  Canvas,
  Fill,
  Skia,
  Image,
  PaintStyle,
  Path,
} from "@shopify/react-native-skia";
import { Gesture, GestureDetector } from "react-native-gesture-handler";
import { useSharedValue } from "react-native-reanimated";
import { useDevice } from "react-native-wgpu";

import { createDemo, SIZE } from "./Lib";

const { width } = Dimensions.get("window");

const paint = Skia.Paint();
paint.setColor(Skia.Color("black"));
paint.setStyle(PaintStyle.Stroke);
paint.setStrokeWidth(0.5);

const grid = Skia.Path.Make();
const cellSize = width / SIZE;

grid.moveTo(0, 0);

// Draw vertical lines
for (let i = 0; i <= SIZE; i++) {
  grid.moveTo(i * cellSize, 0);
  grid.lineTo(i * cellSize, width);
}

// Draw horizontal lines
for (let i = 0; i <= SIZE; i++) {
  grid.moveTo(0, i * cellSize);
  grid.lineTo(width, i * cellSize);
}

const f = 1 / cellSize;

export function MNISTInference() {
  const { device } = useDevice();
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
  useEffect(() => {
    (async () => {
      if (device) {
        createDemo(device);
      }
    })();
  }, [device]);
  return (
    <View style={style.container}>
      <GestureDetector gesture={gesture}>
        <Canvas style={style.canvas}>
          <Fill color="rgb(239, 239, 248)" />
          <Path
            path={grid}
            style="stroke"
            color="rgb(209, 209, 209)"
            strokeWidth={1}
          />
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
