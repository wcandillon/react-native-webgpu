import { useEffect, useRef, useState } from "react";
import { PixelRatio } from "react-native";
import {
  useGPUContext,
  warnIfNotHardwareAccelerated,
  type NativeCanvas,
} from "react-native-wgpu";

interface SceneProps {
  context: GPUCanvasContext;
  device: GPUDevice;
  gpu: GPU;
  presentationFormat: GPUTextureFormat;
  canvas: NativeCanvas;
}

type RenderScene = (timestamp: number) => void;
type Scene = (props: SceneProps) => RenderScene | void | Promise<RenderScene>;

export const useWebGPU = (scene: Scene) => {
  const [adapter, setAdapter] = useState<GPUAdapter | null>(null);
  const [device, setDevice] = useState<GPUDevice | null>(null);
  const { ref, context } = useGPUContext();
  const animationFrameId = useRef<number | null>(null);
  useEffect(() => {
    (async () => {
      if (!adapter) {
        const adp = await navigator.gpu.requestAdapter();
        if (!adp) {
          throw new Error("No appropriate GPUAdapter found.");
        }
        setAdapter(adp);
        warnIfNotHardwareAccelerated(adp);
        return;
      }
      if (!device) {
        const dev = await adapter.requestDevice();
        if (!dev) {
          throw new Error("No appropriate GPUDevice found.");
        }
        setDevice(dev);
        return;
      }
      if (!context) {
        return;
      }

      const canvas = context.canvas as HTMLCanvasElement;
      const presentationFormat = navigator.gpu.getPreferredCanvasFormat();
      canvas.width = canvas.clientWidth * PixelRatio.get();
      canvas.height = canvas.clientHeight * PixelRatio.get();
      context.configure({
        device,
        format: presentationFormat,
        alphaMode: "opaque",
      });

      const sceneProps: SceneProps = {
        context,
        device,
        gpu: navigator.gpu,
        presentationFormat,
        canvas: context.canvas as unknown as NativeCanvas,
      };

      const r = scene(sceneProps);
      let renderScene: RenderScene;
      if (r instanceof Promise) {
        renderScene = await r;
      } else {
        renderScene = r as RenderScene;
      }
      if (typeof renderScene === "function") {
        const render = () => {
          const timestamp = Date.now();
          renderScene(timestamp);
          context.present();
          animationFrameId.current = requestAnimationFrame(render);
        };

        animationFrameId.current = requestAnimationFrame(render);
      }
    })();
    return () => {
      if (animationFrameId.current) {
        cancelAnimationFrame(animationFrameId.current);
      }
    };
  }, [adapter, context, device, scene]);
  return ref;
};
