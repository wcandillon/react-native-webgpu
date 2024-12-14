import React, { useCallback, useEffect, useRef } from "react";
import { Button, Dimensions, Platform, StyleSheet, View } from "react-native";
import {
  Canvas,
  Fill,
  Skia,
  PaintStyle,
  Path,
  ColorType,
  AlphaType,
  matchFont,
  Text,
  notifyChange,
  Group,
} from "@shopify/react-native-skia";
import { Gesture, GestureDetector } from "react-native-gesture-handler";
import type { SharedValue } from "react-native-reanimated";
import { runOnJS, useSharedValue } from "react-native-reanimated";
import { useDevice } from "react-native-wgpu";

import type { Network } from "./Lib";
import { createDemo, centerData, SIZE } from "./Lib";

const { width } = Dimensions.get("window");

const fontFamily = Platform.select({ ios: "Helvetica", default: "serif" });
const fontStyle = {
  fontFamily,
  fontSize: 200,
};
const font = matchFont(fontStyle);

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
const surface = Skia.Surface.MakeOffscreen(SIZE, SIZE)!;
const canvas = surface.getCanvas();

export function MNISTInference() {
  const { device } = useDevice();
  const network = useRef<Network>();
  const text = useSharedValue("");
  const path = useSharedValue(Skia.Path.Make());
  const runInference = useCallback(
    async (data: number[]) => {
      if (network.current === undefined) {
        return;
      }
      const certainties = await network.current.inference(data);
      const max = Math.max(...certainties);
      const index = certainties.indexOf(max);
      text.value = `${index}`;
    },
    [text],
  );
  const gesture = Gesture.Pan()
    .onStart((e) => {
      path.value.moveTo(e.x * f, e.y * f);
    })
    .onChange((e) => {
      path.value.lineTo(e.x * f, e.y * f);
      canvas.drawPath(path.value, paint);
      const pixels = canvas.readPixels(0, 0, {
        width: SIZE,
        height: SIZE,
        alphaType: AlphaType.Opaque,
        colorType: ColorType.Alpha_8,
      })!;
      notifyChange(path as SharedValue<unknown>);
      runOnJS(runInference)(
        centerData(pixels as Uint8Array).map((x) => (x / 255) * 3.24 - 0.42),
      );
    });
  useEffect(() => {
    (async () => {
      if (device) {
        const demo = await createDemo(device);
        network.current = demo.network;
      }
    })();
  }, [device, network]);
  return (
    <View style={style.container}>
      <Button
        onPress={() => {
          canvas.clear(Skia.Color("transparent"));
          path.value = Skia.Path.Make();
          text.value = "";
        }}
        title="Reset"
      />
      <GestureDetector gesture={gesture}>
        <Canvas style={style.canvas}>
          <Fill color="rgb(239, 239, 248)" />
          <Path
            path={grid}
            style="stroke"
            color="rgb(209, 209, 209)"
            strokeWidth={1}
          />
          <Group transform={[{ scale: width / SIZE }]}>
            <Path
              path={path}
              style="stroke"
              color="black"
              strokeWidth={1}
              strokeCap="round"
            />
          </Group>
          <Text text={text} x={(width - 100) / 2} y={1.5 * width} font={font} />
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
    height: width * 2,
  },
});
