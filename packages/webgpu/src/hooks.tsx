import type { ReactNode } from "react";
import { createContext, useContext, useEffect, useRef, useState } from "react";

import type { RNCanvasContext, CanvasRef, NativeCanvas } from "./Canvas";

type Unsubscribe = () => void;

export const warnIfNotHardwareAccelerated = (adapter: GPUAdapter) => {
  if (adapter.info.architecture === "swiftshader") {
    console.warn(
      "GPUAdapter is not hardware accelerated. This is common on Android emulators. Rendering will be slow.",
    );
  }
};

interface DeviceContext {
  device: GPUDevice | null;
  adapter: GPUAdapter | null;
}

const DeviceContext = createContext<DeviceContext | null>(null);

interface DeviceProviderProps {
  children?: ReactNode | ReactNode[];
  adapterOptions?: GPURequestAdapterOptions;
  deviceDescriptor?: GPUDeviceDescriptor;
}

export const GPUDeviceProvider = ({
  children,
  adapterOptions,
  deviceDescriptor,
}: DeviceProviderProps) => {
  const state = useDevice(adapterOptions, deviceDescriptor);
  if (!state.device) {
    return null;
  }
  return (
    <DeviceContext.Provider value={state}>{children}</DeviceContext.Provider>
  );
};

export const useSurface = () => {
  const [surface, setSurface] = useState<NativeCanvas | null>(null);
  const ref = useCanvasEffect(() => {
    const sur = ref.current!.getNativeSurface();
    setSurface(sur);
  });
  return { ref, surface };
};

export const useMainDevice = () => {
  const ctx = useContext(DeviceContext);
  if (!ctx) {
    throw new Error("No DeviceContext found.");
  }
  return ctx;
};

export const useDevice = (
  adapterOptions?: GPURequestAdapterOptions,
  deviceDescriptor?: GPUDeviceDescriptor,
) => {
  const [state, setState] = useState<DeviceContext | null>(null);
  useEffect(() => {
    (async () => {
      if (!state) {
        const adapter = await navigator.gpu.requestAdapter(adapterOptions);
        if (!adapter) {
          throw new Error("No appropriate GPUAdapter found.");
        }
        warnIfNotHardwareAccelerated(adapter);
        const device = await adapter.requestDevice(deviceDescriptor);
        if (!device) {
          throw new Error("No appropriate GPUDevice found.");
        }
        setState({ adapter, device });
        return;
      }
    })();
  }, [adapterOptions, deviceDescriptor, state]);
  return { ...state };
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
