import { useEffect, useRef } from "react";

type Unsubscribe = () => void;

export const useCanvasEffect = (
  effect: (payload: {
    device: GPUDevice;
  }) => void | Unsubscribe | Promise<void | Unsubscribe>,
) => {
  const unsubscribe = useRef<Unsubscribe>();
  useEffect(() => {
    requestAnimationFrame(async () => {
      const adapter = await navigator.gpu.requestAdapter();
      if (!adapter) {
        return;
      }
      const device = await adapter.requestDevice();
      const unsub = await effect({ device });
      if (unsub) {
        unsubscribe.current = unsub;
      }
    });
    return () => {
      if (unsubscribe.current) {
        unsubscribe.current();
      }
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);
};
