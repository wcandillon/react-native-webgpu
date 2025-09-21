import { useEffect } from "react";
import { PixelRatio } from "react-native";
import { useGPUContext, useDevice, type NativeCanvas } from "react-native-wgpu";

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
  const { device } = useDevice();
  const { ref, context } = useGPUContext();
  useEffect(() => {
    if (!context || !device) {
      return undefined; // No cleanup
    }

    const canvas = context.canvas as HTMLCanvasElement;
    const presentationFormat = navigator.gpu.getPreferredCanvasFormat();
    canvas.width = canvas.clientWidth * PixelRatio.get();
    canvas.height = canvas.clientHeight * PixelRatio.get();
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

    const r = scene(sceneProps);

    let handle: number | undefined;
    const abortCtrl = new AbortController();

    (async () => {
      let renderScene: RenderScene;
      if (r && "then" in r) {
        renderScene = await r;
      } else {
        renderScene = r as RenderScene;
      }

      if (typeof renderScene !== "function" || abortCtrl.signal.aborted) {
        // No scene to render, or already aborted
        return;
      }

      const render = () => {
        const timestamp = Date.now();
        renderScene(timestamp);
        context.present();
        handle = requestAnimationFrame(render);
      };

      render();
    })();

    return () => {
      abortCtrl.abort();
      if (handle !== undefined) {
        cancelAnimationFrame(handle);
      }
    };
  }, [context, device, scene]);
  return ref;
};
