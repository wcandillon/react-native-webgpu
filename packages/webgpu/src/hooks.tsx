import type { ReactNode } from "react";
import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useRef,
  useState,
} from "react";

import type { RNCanvasContext, CanvasRef, NativeCanvas } from "./Canvas";

type Unsubscribe = () => void;

export const warnIfNotHardwareAccelerated = (adapter: GPUAdapter) => {
  if (adapter.isFallbackAdapter) {
    console.warn(
      // eslint-disable-next-line max-len
      "GPUAdapter is not hardware accelerated. This is common on Android emulators. Rendering will be slow. Some features may be unavailable.",
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
    setState(null); // resetting old adapter and device

    let deviceContext: Promise<DeviceContext> | DeviceContext = (async () => {
      const adapter = await navigator.gpu.requestAdapter(adapterOptions);
      if (!adapter) {
        throw new Error("No appropriate GPUAdapter found.");
      }
      warnIfNotHardwareAccelerated(adapter);
      const device = await adapter.requestDevice(deviceDescriptor);
      if (!device) {
        throw new Error("No appropriate GPUDevice found.");
      }
      deviceContext = { adapter, device };
      setState(deviceContext);
      return deviceContext;
    })();

    return () => {
      if (deviceContext instanceof Promise) {
        deviceContext.then((dev) => dev.device?.destroy());
      } else {
        deviceContext.device?.destroy();
      }
    };
  }, [adapterOptions, deviceDescriptor]);

  return { adapter: state?.adapter ?? null, device: state?.device ?? null };
};

export const useGPUContext = () => {
  const [context, setContext] = useState<RNCanvasContext | null>(null);
  const ref = useCanvasEffect(
    useCallback((canvas) => {
      const ctx = canvas.getContext("webgpu")!;
      setContext(ctx);
    }, []),
  );
  return { ref, context };
};

export const useCanvasEffect = (
  effect: (
    canvas: CanvasRef,
  ) => void | Unsubscribe | Promise<Unsubscribe | void> | Promise<void>,
) => {
  const ref = useRef<CanvasRef>(null);

  useEffect(() => {
    const canvas = ref.current;
    if (!canvas || !canvas.whenReady) {
      throw new Error("The reference is not assigned to a WebGPU Canvas");
    }
    let mounted = true;
    let unsub: Unsubscribe | undefined | Promise<Unsubscribe | void>;

    canvas.whenReady(() => {
      if (!mounted) {
        return;
      }
      const sub = effect(canvas);
      if (sub) {
        unsub = sub;
      }
    });

    return () => {
      mounted = false;
      if (unsub) {
        if (unsub instanceof Promise) {
          unsub.then((sub) => sub?.());
        } else {
          unsub();
        }
      }
    };
  }, [effect]);

  return ref;
};
