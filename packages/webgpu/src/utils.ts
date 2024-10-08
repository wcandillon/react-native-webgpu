import type { DependencyList } from "react";
import { useEffect, useRef, useState } from "react";

import type { RNCanvasContext } from "./Canvas";
import type { CanvasRef } from "./Canvas";

type Unsubscribe = () => void;

export const warnIfNotHardwareAccelerated = (adapter: GPUAdapter) => {
  if (adapter.info.architecture === "swiftshader") {
    console.warn(
      "GPUAdapter is not hardware accelerated. This is common on Android emulators. Rendering will be slow.",
    );
  }
};

export const useGPUContextEffect = (
  effect: (ctx: RNCanvasContext) => void | Unsubscribe | Promise<void | Unsubscribe>,
  deps?: DependencyList
) => {
  const [context, setContext] = useState<RNCanvasContext | null>(null);
  const ref = useCanvasEffect(() => {
    const ctx = ref.current!.getContext("webgpu")!;
    setContext(ctx);
  });
  useEffect(() => {
    if (context) {
      effect(context);
    }
  }, [context, ...(deps ?? [])])
  return {ref, context};
}

// TODO: add example en fabric that uses useEffect or useLayoutEffect directly
export const useCanvasEffect = (
  effect: () => void
) => {
  const ref = useRef<CanvasRef>(null);
  useEffect(() => {
    ref.current!.whenReady(async () => {
      effect();
    });
  }, []);
  return ref;
};
