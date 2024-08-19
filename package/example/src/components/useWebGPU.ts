import { useEffect, useRef, useState } from "react";
import { PixelRatio } from "react-native";
import { useFrameCallback } from "react-native-reanimated";
import {
  warnIfNotHardwareAccelerated,
  type CanvasRef,
  type NativeCanvas,
} from "react-native-wgpu";

const useDevice = () => {
  const [device, setDevice] = useState<GPUDevice | null>(null);
  useEffect(() => {
    (async () => {
      const adapter = await navigator.gpu.requestAdapter();
      if (!adapter) {
        throw new Error("No appropriate GPUAdapter found.");
      }
      warnIfNotHardwareAccelerated(adapter);
      const dev = await adapter.requestDevice();
      if (!dev) {
        throw new Error("No appropriate GPUDevice found.");
      }
      setDevice(dev);
    })();
  }, []);
  return {
    device,
  };
};

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
  const canvasRef = useRef<CanvasRef>(null);
  const { device } = useDevice();
  useEffect(() => {
    let animationFrameId: number;
    (async () => {
      let frameNumber = 0;

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
      let renderScene;
      if (r instanceof Promise) {
        renderScene = await r;
      } else {
        renderScene = r;
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
          animationFrameId = requestAnimationFrame(render);
        };

        animationFrameId = requestAnimationFrame(render);
      }
    })();
    return () => {
      if (animationFrameId) {
        cancelAnimationFrame(animationFrameId);
      }
    };
  }, [scene, canvasRef, device]);

  return { canvasRef };
};
