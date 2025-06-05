import React, { useCallback, useEffect, useMemo, useRef } from "react";
import { Button, Dimensions, StyleSheet, View } from "react-native";
import type {
  SkImage,
  SkPaint,
  SkPath,
  SkSurface,
} from "@shopify/react-native-skia";
import {
  useFont,
  Canvas,
  Fill,
  Skia,
  PaintStyle,
  Path,
  ColorType,
  AlphaType,
  Text,
  Image,
  FilterMode,
} from "@shopify/react-native-skia";
import { Gesture, GestureDetector } from "react-native-gesture-handler";
import { runOnJS, runOnUI, useSharedValue } from "react-native-reanimated";
import { useDevice } from "react-native-wgpu";

import type { Network } from "./Lib";
import { createDemo, centerData, SIZE } from "./Lib";

export function MNISTInference() {
  const skiaConstants = useRef<null | {
    paint: SkPaint;
    grid: SkPath;
    width: number;
    f: number;
  }>(null);

  const font = useFont(require("../assets/helvetica.ttf"));

  // Lazy initialize skia derived constants
  if (!skiaConstants.current) {
    const { width } = Dimensions.get("window");

    const paint = Skia.Paint();
    paint.setColor(Skia.Color("black"));
    paint.setStyle(PaintStyle.Stroke);
    paint.setStrokeWidth(1);

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

    skiaConstants.current = {
      f,
      paint,
      grid,
      width,
    };
  }

  const { f, paint, grid, width } = skiaConstants.current;

  const { device } = useDevice();
  const network = useRef<Network>();
  const text = useSharedValue("");
  const path = useSharedValue(Skia.Path.Make());
  const surface = useSharedValue<SkSurface | null>(null);
  const image = useSharedValue<SkImage | null>(null);
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

  const gesture = useMemo(() => {
    return Gesture.Pan()
      .onStart((e) => {
        path.value.moveTo(e.x * f, e.y * f);
      })
      .onChange((e) => {
        path.value.lineTo(e.x * f, e.y * f);
        if (surface.value) {
          const canvas = surface.value.getCanvas();
          canvas.drawPath(path.value, paint);
          surface.value.flush();
          image.value = surface.value!.makeImageSnapshot();
          const pixels = image.value.readPixels(0, 0, {
            width: SIZE,
            height: SIZE,
            colorType: ColorType.RGBA_8888,
            alphaType: AlphaType.Unpremul,
          });

          const gray = new Uint8Array(SIZE * SIZE);
          for (let i = 0; i < SIZE * SIZE; i++) {
            gray[i] = pixels![i * 4];
          }

          runOnJS(runInference)(
            centerData(gray).map((x) => (x / 255) * 3.24 - 0.42),
          );
        }
      });
  }, [path, runInference, surface, image, f, paint]);

  useEffect(() => {
    (async () => {
      if (device) {
        const demo = await createDemo(device);
        network.current = demo.network;
      }
      runOnUI(() => {
        surface.value = Skia.Surface.MakeOffscreen(SIZE, SIZE)!;
      })();
    })();
  }, [device, network, surface]);

  if (!font) {
    return null;
  }

  return (
    <View style={style.container}>
      <Button
        onPress={() => {
          surface.value?.getCanvas().clear(Skia.Color("transparent"));
          image.value = null;
          path.value = Skia.Path.Make();
          text.value = "";
        }}
        title="Reset"
      />
      <GestureDetector gesture={gesture}>
        <Canvas style={{ width, height: width * 2 }}>
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
            sampling={{ filter: FilterMode.Nearest }}
          />
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
});
