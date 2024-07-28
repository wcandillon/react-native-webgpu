import { useEffect, useRef, useState } from "react";
import type { CanvasRef } from "react-native-webgpu";

const useDevice = () => {
  const [device, setDevice] = useState<GPUDevice | null>(null);
  useEffect(() => {
    (async () => {
      const adapter = await navigator.gpu.requestAdapter();
      if (!adapter) {
        throw new Error("No appropriate GPUAdapter found.");
      }
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
  canvas: OffscreenCanvas;
}

type RenderScene = (timestamp: number) => void;
type Scene = (props: SceneProps) => RenderScene | void;

export const useWebGPU = (scene: Scene) => {
  const canvasRef = useRef<CanvasRef>(null);
  const { device } = useDevice();
  useEffect(() => {
    let animationFrameId: number;

    if (!device) {
      return;
    }
    const canvas = canvasRef.current;
    if (!canvas) {
      return;
    }

    const context = canvas.getContext("webgpu")!;
    const presentationFormat = navigator.gpu.getPreferredCanvasFormat();

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
      canvas: context.canvas as OffscreenCanvas,
    };

    const renderScene = scene(sceneProps);

    const render = () => {
      const timestamp = Date.now();
      if (typeof renderScene === "function") {
        renderScene(timestamp);
      }
      device.queue.onSubmittedWorkDone().then(() => {
        context.present();
        animationFrameId = requestAnimationFrame(render);
      });
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
