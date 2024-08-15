import { useEffect, useRef } from "react";
import { PixelRatio } from "react-native";
import { useAdapter, useDevice } from "react-native-wgpu";
import type { CanvasRef, NativeCanvas } from "react-native-wgpu";

interface SceneProps {
  context: GPUCanvasContext;
  device: GPUDevice;
  gpu: GPU;
  presentationFormat: GPUTextureFormat;
  canvas: NativeCanvas;
}

type RenderScene = (timestamp: number) => void;
type Scene = (props: SceneProps) => RenderScene | void;

export const useWebGPU = (scene: Scene) => {
  const canvasRef = useRef<CanvasRef>(null);
  const device = useDevice();
  //const adapter = useAdapter();
  useEffect(() => {
    let animationFrameId: number;
    let frameNumber = 0;
    console.log({ device, canvasRef: canvasRef.current });
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

    const renderScene = scene(sceneProps);

    const render = () => {
      frameNumber++;
      const timestamp = Date.now();
      if (typeof renderScene === "function") {
        renderScene(timestamp);
      }
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

    return () => {
      if (animationFrameId) {
        cancelAnimationFrame(animationFrameId);
      }
    };
  }, [scene, canvasRef, device]);

  return { canvasRef };
};
