import { useEffect, useRef } from "react";
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

export const useWebGPUAutoPresent = (scene: Scene) => {
  const { device } = useDevice();
  const { ref, context } = useGPUContext();
  const animationFrameId = useRef<number | null>(null);
  const isAutoPresentEnabled = useRef(false);

  useEffect(() => {
    (async () => {
      if (!context || !device) {
        return;
      }

      const canvas = context.canvas as HTMLCanvasElement;
      const presentationFormat = navigator.gpu.getPreferredCanvasFormat();
      canvas.width = canvas.clientWidth * PixelRatio.get();
      canvas.height = canvas.clientHeight * PixelRatio.get();

      // Check if context has auto-present capability
      const hasAutoPresent = !!(context as any)._markNeedsPresent;
      isAutoPresentEnabled.current = hasAutoPresent;

      context.configure({
        device,
        format: presentationFormat,
        alphaMode: "premultiplied",
      });

      // Hook into device.queue.submit for auto-present contexts
      if (hasAutoPresent) {
        const originalSubmit = device.queue.submit.bind(device.queue);
        device.queue.submit = (commandBuffers: GPUCommandBuffer[]) => {
          const result = originalSubmit(commandBuffers);
          // Mark context as needing present
          (context as any)._markNeedsPresent();
          return result;
        };
      }

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
          
          // Only call present manually if auto-present is not enabled
          if (!isAutoPresentEnabled.current) {
            context.present();
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
      // Cleanup auto-present context
      if (isAutoPresentEnabled.current && (context as any)._cleanup) {
        (context as any)._cleanup();
      }
    };
  }, [context, device, scene]);
  
  return ref;
};