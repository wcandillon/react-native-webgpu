import { useEffect, useRef, useState } from "react";

import type { RNCanvasContext, CanvasRef } from "./Canvas";

type Unsubscribe = () => void;

export const warnIfNotHardwareAccelerated = (adapter: GPUAdapter) => {
  if (adapter.info.architecture === "swiftshader") {
    console.warn(
      "GPUAdapter is not hardware accelerated. This is common on Android emulators. Rendering will be slow.",
    );
  }
};

export const useGPUContext = () => {
  const [context, setContext] = useState<RNCanvasContext | null>(null);
  const ref = useCanvasEffect(() => {
    const ctx = ref.current!.getContext("webgpu")!;
    setContext(ctx);
  });
  return { ref, context };
};

export const useCanvasEffect = (
  effect: () =>
    | void
    | Unsubscribe
    | Promise<Unsubscribe | void>
    | Promise<void>,
) => {
  const unsub = useRef<Unsubscribe | null | Promise<Unsubscribe | void>>(null);
  const ref = useRef<CanvasRef>(null);
  useEffect(() => {
    ref.current!.whenReady(async () => {
      const sub = effect();
      if (sub) {
        unsub.current = sub;
      }
    });
    return () => {
      if (unsub.current) {
        if (unsub.current instanceof Promise) {
          unsub.current.then((sub) => sub && sub());
        } else {
          unsub.current();
        }
      }
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);
  return ref;
};
