import { useEffect, useRef } from "react";
import { PixelRatio } from "react-native";
import {
  useDevice,
  type CanvasRef,
  type NativeCanvas,
} from "react-native-webgpu";

interface SceneProps {
  context: GPUCanvasContext;
  device: GPUDevice;
  gpu: GPU;
  presentationFormat: GPUTextureFormat;
  canvas: NativeCanvas;
}

type RenderScene = (timestamp: number) => void;
export type Scene = (props: SceneProps) => RenderScene | void | Promise<RenderScene>;
export type { SceneProps };

export const useWebGPU = (scene: Scene) => {
  const { device } = useDevice();
  const ref = useRef<CanvasRef>(null);
  const sceneRef = useRef(scene);
  const animationFrameId = useRef<number | null>(null);

  sceneRef.current = scene;

  useEffect(() => {
    let cancelled = false;

    const start = () => {
      const context = ref.current?.getContext("webgpu");
      if (!context || !device || cancelled) {
        animationFrameId.current = requestAnimationFrame(start);
        return;
      }

      const htmlCanvas = context.canvas as HTMLCanvasElement;
      if (htmlCanvas.clientWidth === 0 || htmlCanvas.clientHeight === 0) {
        animationFrameId.current = requestAnimationFrame(start);
        return;
      }

      const presentationFormat = navigator.gpu.getPreferredCanvasFormat();
      htmlCanvas.width = htmlCanvas.clientWidth * PixelRatio.get();
      htmlCanvas.height = htmlCanvas.clientHeight * PixelRatio.get();
      context.configure({
        device,
        format: presentationFormat,
        alphaMode: "premultiplied",
      });

      const sceneProps: SceneProps = {
        context,
        device,
        gpu: navigator.gpu,
        presentationFormat,
        canvas: context.canvas as unknown as NativeCanvas,
      };

      void (async () => {
        const result = sceneRef.current(sceneProps);
        let renderScene: RenderScene | undefined;
        if (result instanceof Promise) {
          renderScene = await result;
        } else if (typeof result === "function") {
          renderScene = result;
        }

        if (typeof renderScene !== "function" || cancelled) {
          return;
        }

        const render = () => {
          if (cancelled) {
            return;
          }
          renderScene!(Date.now());
          context.present();
          animationFrameId.current = requestAnimationFrame(render);
        };
        animationFrameId.current = requestAnimationFrame(render);
      })();
    };

    animationFrameId.current = requestAnimationFrame(start);

    return () => {
      cancelled = true;
      if (animationFrameId.current) {
        cancelAnimationFrame(animationFrameId.current);
      }
    };
  }, [device]);

  return ref;
};
