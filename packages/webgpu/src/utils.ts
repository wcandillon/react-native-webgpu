import type { DependencyList } from "react";
import { useEffect, useRef } from "react";

import type { CanvasRef } from "./Canvas";

type Unsubscribe = () => void;

export const warnIfNotHardwareAccelerated = (adapter: GPUAdapter) => {
  if (adapter.info.architecture === "swiftshader") {
    console.warn(
      "GPUAdapter is not hardware accelerated. This is common on Android emulators. Rendering will be slow.",
    );
  }
};

export const useCanvasEffect = (
  effect: () => void | Unsubscribe | Promise<void | Unsubscribe>,
  deps: DependencyList = [],
) => {
  const ref = useRef<CanvasRef>(null);
  const unsubscribe = useRef<Unsubscribe>();
  useEffect(() => {
    requestAnimationFrame(async () => {
      // const adapter = await navigator.gpu.requestAdapter();
      // if (!adapter) {
      //   return;
      // }
      // const device = await adapter.requestDevice();
      const unsub = await effect();
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
  }, deps);
  return ref;
};
