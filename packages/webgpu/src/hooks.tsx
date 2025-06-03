import type { ReactNode, RefObject } from "react";
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
  const ref = useCanvasEffect(
    useCallback(({ canvasRef }) => {
      const sur = canvasRef.getNativeSurface();
      setSurface(sur);
    }, []),
  );
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
    let mounted = true;
    (async () => {
      const adapter = await navigator.gpu.requestAdapter(adapterOptions);
      if (!adapter) {
        throw new Error("No appropriate GPUAdapter found.");
      }
      warnIfNotHardwareAccelerated(adapter);
      const device = await adapter.requestDevice(deviceDescriptor);
      if (!device) {
        throw new Error("No appropriate GPUDevice found.");
      }

      if (!mounted || state) {
        // Unmounted or already defined
        return;
      }
      setState({ adapter, device });
    })();

    return () => {
      mounted = false;
    };
  }, [adapterOptions, deviceDescriptor, state]);
  return { adapter: state?.adapter ?? null, device: state?.device ?? null };
};

export function useGPUContext(): {
  ref: RefObject<CanvasRef>;
  context: RNCanvasContext | null;
} {
  const [context, setContext] = useState<RNCanvasContext | null>(null);
  const ref = useCanvasEffect(
    useCallback(({ canvasRef }) => {
      const ctx = canvasRef.getContext("webgpu")!;
      setContext(ctx);
    }, []),
  );
  return { ref, context };
}

type EffectReturn =
  | void
  | Unsubscribe
  | Promise<Unsubscribe | void>
  | Promise<void>;

interface CanvasEffectContext {
  signal: AbortSignal;
  canvasRef: CanvasRef;
}

export function useCanvasEffect(
  effect: (ctx: CanvasEffectContext) => EffectReturn,
): RefObject<CanvasRef> {
  const ref = useRef<CanvasRef>(null);

  useEffect(() => {
    const ctrl = new AbortController();
    let unsub: EffectReturn;

    const canvasRef = ref.current;

    if (!canvasRef || !canvasRef.whenReady) {
      throw new Error("The reference is not assigned to a WebGPU Canvas");
    }

    canvasRef.whenReady(() => {
      if (ctrl.signal.aborted) {
        return;
      }

      unsub = effect({ signal: ctrl.signal, canvasRef });
    });

    return () => {
      ctrl.abort();

      if (unsub) {
        if ("then" in unsub) {
          unsub.then((cb) => cb && cb());
        } else {
          unsub();
        }
      }
    };
  }, [effect]);

  return ref;
}
