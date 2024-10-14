import { useEffect, useRef, useState } from "react";

import type { RNCanvasContext, CanvasRef, NativeCanvas } from "./Canvas";

type Unsubscribe = () => void;

export const warnIfNotHardwareAccelerated = (adapter: GPUAdapter) => {
  if (adapter.info.architecture === "swiftshader") {
    console.warn(
      "GPUAdapter is not hardware accelerated. This is common on Android emulators. Rendering will be slow.",
    );
  }
};

export const useSurface = () => {
  const [surface, setSurface] = useState<NativeCanvas | null>(null);
  const ref = useCanvasEffect(() => {
    const sur = ref.current!.getNativeSurface();
    setSurface(sur);
  });
  return { ref, surface };
};

export const useDevice = (
  adapterOptions?: GPURequestAdapterOptions,
  deviceDescriptor?: GPUDeviceDescriptor,
) => {
  const [adapter, setAdapter] = useState<GPUAdapter | null>(null);
  const [device, setDevice] = useState<GPUDevice | null>(null);
  useEffect(() => {
    (async () => {
      if (!adapter) {
        const adp = await navigator.gpu.requestAdapter(adapterOptions);
        if (!adp) {
          throw new Error("No appropriate GPUAdapter found.");
        }
        setAdapter(adp);
        warnIfNotHardwareAccelerated(adp);
        return;
      }
      if (!device) {
        const dev = await adapter.requestDevice(deviceDescriptor);
        if (!dev) {
          throw new Error("No appropriate GPUDevice found.");
        }
        setDevice(dev);
        return;
      }
    })();
  }, [adapter, adapterOptions, device, deviceDescriptor]);
  return { adapter, device };
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
