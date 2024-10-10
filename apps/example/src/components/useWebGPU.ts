import { useRef } from "react";
import { PixelRatio } from "react-native";
import {
  useCanvasEffect,
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
  const animationFrameId = useRef<number | null>(null);
  const canvasRef = useCanvasEffect(() => {
    (async () => {
      let frameNumber = 0;
      const adapter = await navigator.gpu.requestAdapter();
      if (!adapter) {
        throw new Error("No appropriate GPUAdapter found.");
      }
      warnIfNotHardwareAccelerated(adapter);
      const device = await adapter.requestDevice();
      if (!device) {
        throw new Error("No appropriate GPUDevice found.");
      }
      if (!device) {
        return;
      }
      if (!canvasRef.current) {
        return;
      }

      const context = canvasRef.current.getContext("webgpu")!;
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
          frameNumber++;
          const timestamp = Date.now();
          renderScene(timestamp);

          context.present();
          if (frameNumber > 2500) {
            frameNumber = 0;
            if (gc) {
              gc();
            }
          }
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
  });

  return { canvasRef };
};
