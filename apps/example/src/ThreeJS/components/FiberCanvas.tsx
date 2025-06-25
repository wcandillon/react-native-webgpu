import * as THREE from "three";
import React, { useRef, useState } from "react";
import type { ReconcilerRoot, RootState } from "@react-three/fiber";
import {
  extend,
  createRoot,
  unmountComponentAtNode,
  events,
} from "@react-three/fiber";
import type { ViewProps } from "react-native";
import { PixelRatio } from "react-native";
import { Canvas, useCanvasEffect } from "react-native-wgpu";

import { ReactNativeCanvas } from "./makeWebGPURenderer";

//global.THREE = global.THREE || THREE;

interface FiberCanvasProps {
  children: React.ReactNode;
  style?: ViewProps["style"];
  camera?: THREE.PerspectiveCamera;
  scene?: THREE.Scene;
}

export const FiberCanvas = ({
  children,
  style,
  scene,
  camera,
}: FiberCanvasProps) => {
  const root = useRef<ReconcilerRoot<OffscreenCanvas>>(null!);
  const [frameloop, setFrameloop] = useState<"always" | "never">("never");

  React.useMemo(() => extend(THREE), []);

  const canvasRef = useCanvasEffect(async () => {
    const context = canvasRef.current!.getContext("webgpu")!;

    // eslint-disable-next-line @typescript-eslint/ban-ts-comment
    // @ts-expect-error
    const canvas = new ReactNativeCanvas(context.canvas) as HTMLCanvasElement;
    canvas.width = canvas.clientWidth * PixelRatio.get();
    canvas.height = canvas.clientHeight * PixelRatio.get();
    const size = {
      top: 0,
      left: 0,
      width: canvas.clientWidth,
      height: canvas.clientHeight,
    };

    if (!root.current) {
      root.current = createRoot(canvas);
    }
    root.current.configure({
      size,
      events,
      scene,
      camera,
      gl: () => {
        const renderer = new THREE.WebGPURenderer({
          antialias: true,
          canvas,
          context,
        });
        // eslint-disable-next-line @typescript-eslint/ban-ts-comment
        // @ts-expect-error
        renderer.xr = { addEventListener: () => {} };
        // Initialize the renderer asynchronously
        renderer.init().then(() => {
          setFrameloop("always");
        });
        return renderer;
      },
      frameloop,
      dpr: 1, //PixelRatio.get(),
      onCreated: (state: RootState) => {
        // Renderer is already initialized in the gl function
        const renderFrame = state.gl.render.bind(state.gl);
        state.gl.render = (s: THREE.Scene, c: THREE.Camera) => {
          renderFrame(s, c);
          context?.present();
        };
      },
    });
    root.current.render(children);
    return () => {
      if (canvas != null) {
        unmountComponentAtNode(canvas!);
      }
    };
  });

  // Update the frameloop when it changes
  React.useEffect(() => {
    if (root.current) {
      root.current.configure({ frameloop });
    }
  }, [frameloop]);

  return <Canvas ref={canvasRef} style={style} />;
};
