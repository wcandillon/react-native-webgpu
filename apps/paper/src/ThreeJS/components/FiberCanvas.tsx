import * as THREE from "three/webgpu";
import React, { useRef } from "react";
import type { ReconcilerRoot } from "@react-three/fiber";
import {
  extend,
  createRoot,
  unmountComponentAtNode,
  events,
} from "@react-three/fiber";
import type { ViewProps } from "react-native";
import { PixelRatio, View } from "react-native";
import { Canvas, useCanvasEffect } from "react-native-wgpu";

import { makeWebGPURenderer } from "./makeWebGPURenderer";

interface FiberCanvasProps {
  children: React.ReactNode;
  style?: ViewProps["style"];
}

export const FiberCanvas = ({ children, style }: FiberCanvasProps) => {
  const root = useRef<ReconcilerRoot<OffscreenCanvas>>(null!);

  React.useMemo(() => extend(THREE), []);

  const canvasRef = useCanvasEffect(async () => {
    const context = canvasRef.current!.getContext("webgpu")!;
    const renderer = makeWebGPURenderer(context);
    const canvas = context.canvas as HTMLCanvasElement;
    canvas.width = canvas.clientWidth * PixelRatio.get();
    canvas.height = canvas.clientHeight * PixelRatio.get();
    const size = {
      top: 0,
      left: 0,
      width: canvas.clientWidth,
      height: canvas.clientHeight,
    };

    if (!root.current) {
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      root.current = createRoot<any>(canvasRef.current);
    }
    root.current.configure({ size, events, gl: renderer });
    root.current.render(children);
    return () => {
      if (canvas != null) {
        unmountComponentAtNode(canvas!);
      }
    };
  });

  return (
    <View style={{ flex: 1 }}>
      <Canvas ref={canvasRef} style={style} />
    </View>
  );
};
