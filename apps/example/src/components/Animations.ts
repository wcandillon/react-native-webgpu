import { useCallback, useEffect } from "react";
import type { FrameInfo } from "react-native-reanimated";
import {
  startMapper,
  stopMapper,
  useFrameCallback,
  useSharedValue,
} from "react-native-reanimated";
import type { RNCanvasContext } from "react-native-wgpu";
import { useCanvasEffect } from "react-native-wgpu";

export const useClock = () => {
  const clock = useSharedValue(0);
  const callback = useCallback(
    (info: FrameInfo) => {
      "worklet";
      clock.value = info.timeSinceFirstFrame;
    },
    [clock],
  );
  useFrameCallback(callback);
  return clock;
};

interface BaseGPUContext {
  device: GPUDevice;
  context: RNCanvasContext;
}

export const useAnimatedContext = <T extends BaseGPUContext>(
  cb: (device: GPUDevice, context: RNCanvasContext) => T,
) => {
  const ctx = useSharedValue<T | null>(null);
  const ref = useCanvasEffect(async () => {
    const presentationFormat = navigator.gpu.getPreferredCanvasFormat();
    const adapter = await navigator.gpu.requestAdapter();
    if (!adapter) {
      throw new Error("No adapter");
    }
    const device = await adapter.requestDevice();
    ref.current?.getContext("webgpu")?.configure({
      device,
      format: presentationFormat,
      alphaMode: "premultiplied",
    });
    const canvas = ref.current;
    if (!canvas) {
      throw new Error("No canvas available");
    }
    const context = canvas.getContext("webgpu");
    if (!context) {
      throw new Error("No WebGPU context available");
    }
    ctx.value = cb(device, context);
  });
  return { ctx, ref };
};

export const useAnimatedRenderer = <
  Ctx extends BaseGPUContext,
  Values extends object,
>(
  init: (device: GPUDevice, context: RNCanvasContext) => Ctx,
  frame: (ctx: Ctx, values: Values) => void,
  values: Values,
  libs: string[] = [],
) => {
  const firstFrame = useSharedValue(true);
  const { ctx, ref } = useAnimatedContext(init);
  useEffect(() => {
    const mapperId = startMapper(() => {
      "worklet";
      if (!ctx.value) {
        return;
      }
      if (firstFrame.value) {
        libs.forEach((lib) => {
          // eslint-disable-next-line no-eval
          eval(lib);
        });
      }
      frame(ctx.value, values);
      firstFrame.value = false;
    }, Object.values(values));
    return () => {
      stopMapper(mapperId);
    };
  }, [ctx.value, firstFrame, frame, libs, values]);
  return { ref };
};
