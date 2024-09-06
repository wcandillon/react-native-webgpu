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
import type { NativeCanvas } from "react-native-wgpu";
import { Canvas, useCanvasEffect } from "react-native-wgpu";

import { makeWebGPURenderer, ReactNativeCanvas } from "./makeWebGPURenderer";

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
    await renderer.init();
    const renderFrame = renderer.render.bind(renderer);
    renderer.render = (scene: THREE.Scene, camera: THREE.Camera) => {
      renderFrame(scene, camera);
      context?.present();
    };

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
      root.current = createRoot<any>(
        new ReactNativeCanvas(context.canvas as NativeCanvas),
      );
    }
    root.current.configure({ size, events, gl: renderer, frameloop: "never" });
    root.current.render(children);
    return () => {
      if (canvas != null) {
        unmountComponentAtNode(canvas!);
      }
    };
  });

  return <Canvas ref={canvasRef} style={style} />;
};
