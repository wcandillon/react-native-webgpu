import * as THREE from "three/webgpu";
import React, { useEffect, useRef } from "react";
import type { ReconcilerRoot, RootState } from "@react-three/fiber";
import {
  createRoot,
  events,
  extend,
  unmountComponentAtNode,
} from "@react-three/fiber";
import type { ViewProps } from "react-native";
import { PixelRatio } from "react-native";
import { Canvas, type CanvasRef, type NativeCanvas } from "react-native-webgpu";

import {
  makeWebGPURenderer,
  ReactNativeCanvas,
} from "@/lib/make-webgpu-renderer";

const extendThree = extend as (catalogue: Record<string, unknown>) => void;

interface FiberCanvasProps {
  children: React.ReactNode;
  style?: ViewProps["style"];
  camera?: THREE.PerspectiveCamera;
  scene?: THREE.Scene;
}

export function FiberCanvas({
  children,
  style,
  scene,
  camera,
}: FiberCanvasProps) {
  const root = useRef<ReconcilerRoot<OffscreenCanvas>>(null!);
  React.useMemo(() => extendThree(THREE), []);
  const canvasRef = useRef<CanvasRef>(null);

  useEffect(() => {
    let cancelled = false;
    let initialized = false;
    const context = canvasRef.current?.getContext("webgpu");
    if (!context) return;

    const renderer = makeWebGPURenderer(context);
    const canvas = new ReactNativeCanvas(
      context.canvas as unknown as NativeCanvas,
    ) as unknown as HTMLCanvasElement;
    canvas.width = canvas.clientWidth * PixelRatio.get();
    canvas.height = canvas.clientHeight * PixelRatio.get();

    if (!root.current) {
      root.current = createRoot(canvas);
    }
    root.current.configure({
      size: {
        top: 0,
        left: 0,
        width: canvas.clientWidth,
        height: canvas.clientHeight,
      },
      events,
      scene,
      camera,
      gl: renderer,
      frameloop: "always",
      dpr: 1,
      onCreated: async (state: RootState) => {
        const webGPURenderer = state.gl as unknown as THREE.WebGPURenderer;
        await webGPURenderer.init();
        if (cancelled) {
          webGPURenderer.dispose();
          return;
        }
        initialized = true;
        const renderFrame = webGPURenderer.render.bind(webGPURenderer);
        webGPURenderer.render = (
          nextScene: THREE.Scene,
          nextCamera: THREE.Camera,
        ) => {
          renderFrame(nextScene, nextCamera);
          context.present();
        };
      },
    });
    root.current.render(children);

    return () => {
      cancelled = true;
      unmountComponentAtNode(canvas);
      if (initialized) {
        renderer.dispose();
      }
    };
  }, [camera, children, scene]);

  return <Canvas ref={canvasRef} style={style} />;
}
