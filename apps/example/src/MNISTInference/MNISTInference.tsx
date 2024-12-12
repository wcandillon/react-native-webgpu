import React, { useCallback, useEffect, useRef } from "react";
import { Dimensions, StyleSheet, View } from "react-native";
import type { SkImage, SkSurface } from "@shopify/react-native-skia";
import {
  Canvas,
  Fill,
  Skia,
  Image,
  PaintStyle,
  Path,
  ColorType,
  AlphaType,
} from "@shopify/react-native-skia";
import { Gesture, GestureDetector } from "react-native-gesture-handler";
import { runOnJS, runOnUI, useSharedValue } from "react-native-reanimated";
import { useDevice } from "react-native-wgpu";

import type { Network } from "./Lib";
import { createDemo, centerData, SIZE } from "./Lib";

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
  const network = useRef<Network>();
  const surface = useSharedValue<SkSurface | null>(null);
  const path = useSharedValue(Skia.Path.Make());
  const image = useSharedValue<SkImage | null>(null);
  const runInference = useCallback(async (data: number[]) => {
    if (network.current === undefined) {
      return;
    }
    const certainties = await network.current.inference(data);
    const max = Math.max(...certainties);
    const index = certainties.indexOf(max);
    const sum = certainties.reduce((a, b) => a + b, 0);
    const normalized = certainties.map((x) => x / sum);
    console.log("Result:", index, normalized);
  }, []);
  const gesture = Gesture.Pan()
    .onStart((e) => {
      path.value.moveTo(e.x * f, e.y * f);
    })
    .onChange((e) => {
      path.value.lineTo(e.x * f, e.y * f);
      if (surface.value === null) {
        return;
      }
      console.log("draw");
      surface.value.getCanvas().drawPath(path.value, paint);
      surface.value.flush();
      image.value = surface.value.makeImageSnapshot();
      const pixels = surface.value.getCanvas().readPixels(0, 0, {
        width: SIZE,
        height: SIZE,
        alphaType: AlphaType.Opaque,
        colorType: ColorType.Alpha_8,
      });
      console.log(pixels?.slice(0, 16));
      console.log(pixels?.length === SIZE * SIZE);
      runOnJS(runInference)(
        centerData(pixels!).map((x) => (x / 255) * 3.24 - 0.42),
      );
    });
  useEffect(() => {
    runOnUI(() => {
      surface.value = Skia.Surface.MakeOffscreen(SIZE, SIZE)!;
    })();
    (async () => {
      if (device) {
        const demo = await createDemo(device);
        network.current = demo;
      }
    })();
  }, [device, network, surface]);
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
