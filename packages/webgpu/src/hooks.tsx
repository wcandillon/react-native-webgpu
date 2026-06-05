import React, {
  createContext,
  useContext,
  useEffect,
  useLayoutEffect,
  useRef,
  useState,
} from "react";
import type { ReactNode } from "react";

import type { CanvasRef, NativeCanvas } from "./Canvas";

export const warnIfNotHardwareAccelerated = (adapter: GPUAdapter) => {
  // Check if adapter is a fallback adapter using the new GPUAdapterInfo API
  // Note: isFallbackAdapter was moved from GPUAdapter to GPUAdapterInfo in Chrome 140
  if (adapter.info && adapter.info.isFallbackAdapter) {
    console.warn(
      "GPUAdapter is not hardware accelerated. This is common on Android emulators, which default to the SwiftShader software renderer. Rendering will be slow and some features may be unavailable. On Apple Silicon you can run the emulator on the host GPU via MoltenVK: use a system image at API level 35 or lower and launch it with `ANDROID_EMU_VK_ICD=moltenvk emulator -avd <name> -gpu host`. See the \"Android Emulators\" section of the React Native WebGPU README for details.",
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
  const ref = useRef<CanvasRef>(null);
  const [surface, setSurface] = useState<NativeCanvas | null>(null);
  useLayoutEffect(() => {
    const sur = ref.current!.getNativeSurface();
    setSurface(sur);
  }, []);
  return { ref, surface };
};

export const useMainDevice = () => {
  const ctx = useContext(DeviceContext);
  if (!ctx) {
    throw new Error("No DeviceContext found.");
  }
  return ctx;
};

export const useCanvasRef = () => useRef<CanvasRef>(null);

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
  return { adapter: state?.adapter ?? null, device: state?.device ?? null };
};
