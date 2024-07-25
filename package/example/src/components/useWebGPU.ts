import { useEffect, useRef, useState } from "react";

interface SceneProps {
  context: GPUCanvasContext;
  device: GPUDevice;
  adapter: GPUAdapter;
  gpu: GPU;
  presentationFormat: GPUTextureFormat;
}

type RenderScene = (timestamp: number) => void;
type Scene = (props: SceneProps) => RenderScene | void;

export const useWebGPU = (scene: Scene) => {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [isReady, setIsReady] = useState(false);

  useEffect(() => {
    let animationFrameId: number;
    let renderScene: RenderScene | void;

    const initWebGPU = async () => {
      const canvas = canvasRef.current;
      if (!canvas) {
        return;
      }

      if (!navigator.gpu) {
        throw new Error("WebGPU not supported on this browser.");
      }

      const { gpu } = navigator;
      const adapter = await gpu.requestAdapter();
      if (!adapter) {
        throw new Error("No appropriate GPUAdapter found.");
      }

      const device = await adapter.requestDevice();
      const context = canvas.getContext("webgpu") as GPUCanvasContext;
      const presentationFormat = navigator.gpu.getPreferredCanvasFormat();

      context.configure({
        device,
        format: presentationFormat,
        alphaMode: "opaque",
      });

      const sceneProps: SceneProps = {
        context,
        device,
        adapter,
        gpu,
        presentationFormat,
      };

      renderScene = scene(sceneProps);
      setIsReady(true);
    };

    initWebGPU();

    const render = (timestamp: number) => {
      if (typeof renderScene === "function") {
        renderScene(timestamp);
      }
      animationFrameId = requestAnimationFrame(render);
    };

    if (isReady) {
      animationFrameId = requestAnimationFrame(render);
    }

    return () => {
      if (animationFrameId) {
        cancelAnimationFrame(animationFrameId);
      }
    };
  }, [scene, isReady]);

  return { canvasRef, isReady };
};
