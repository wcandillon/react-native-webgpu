import { useEffect, useLayoutEffect, useRef, useState } from "react";

import type { CanvasRef, NativeCanvas } from "./Canvas";

export const warnIfNotHardwareAccelerated = (adapter: GPUAdapter) => {
  // Check if adapter is a fallback adapter using the new GPUAdapterInfo API
  // Note: isFallbackAdapter was moved from GPUAdapter to GPUAdapterInfo in Chrome 140
  if (adapter.info && adapter.info.isFallbackAdapter) {
    console.warn(
      "GPUAdapter is not hardware accelerated. This is common on Android emulators. Rendering will be slow. Some features may be unavailable.",
    );
  }
};

interface DeviceState {
  device: GPUDevice | null;
  adapter: GPUAdapter | null;
}

export const useSurface = () => {
  const ref = useRef<CanvasRef>(null);
  const [surface, setSurface] = useState<NativeCanvas | null>(null);
  useLayoutEffect(() => {
    const sur = ref.current!.getNativeSurface();
    setSurface(sur);
  }, []);
  return { ref, surface };
};

export const useCanvasRef = () => useRef<CanvasRef>(null);

export const useDevice = (
  adapterOptions?: GPURequestAdapterOptions,
  deviceDescriptor?: GPUDeviceDescriptor,
) => {
  const [state, setState] = useState<DeviceState | null>(null);
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
