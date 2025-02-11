import { useCallback } from "react";
import type { FrameInfo } from "react-native-reanimated";
import { useFrameCallback, useSharedValue } from "react-native-reanimated";
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
