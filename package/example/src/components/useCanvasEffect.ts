import { useEffect } from "react";

export const useCanvasEffect = (
  effect: (payload: {
    device: GPUDevice;
  }) => void | (() => void) | Promise<void | (() => void)>,
) => {
  useEffect(() => {
    requestAnimationFrame(async () => {
      const adapter = await navigator.gpu.requestAdapter();
      if (!adapter) {
        return;
      }
      const device = await adapter.requestDevice();
      effect({ device });
    });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);
};
