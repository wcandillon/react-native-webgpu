import type { ReactNode } from "react";
import { createContext, useContext, useEffect, useState } from "react";
import { Platform } from "react-native";

export const warnIfNotHardwareAccelerated = (adapter: GPUAdapter) => {
  if (
    Platform.OS === "android" &&
    adapter.info.architecture === "swiftshader"
  ) {
    console.warn(
      "GPUAdapter is not hardware accelerated. This is common on Android emulators. Rendering will be slow.",
    );
  }
};

interface DeviceContext {
  device: GPUDevice;
  adapter: GPUAdapter;
}

const DeviceContext = createContext<DeviceContext | null>(null);

export const useDevice = () => {
  const ctx = useContext(DeviceContext);
  if (!ctx) {
    throw new Error("No DeviceContext found.");
  }
  return ctx.device;
};

export const useAdapter = () => {
  const ctx = useContext(DeviceContext);
  if (!ctx) {
    throw new Error("No DeviceContext found.");
  }
  return ctx.adapter;
};

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
  const [adapter, setAdapter] = useState<GPUAdapter | null>(null);
  const [device, setDevice] = useState<GPUDevice | null>(null);
  useEffect(() => {
    (async () => {
      const adapt = await navigator.gpu.requestAdapter(adapterOptions);
      if (!adapt) {
        throw new Error("No appropriate GPUAdapter found.");
      }
      warnIfNotHardwareAccelerated(adapt);
      const dev = await adapt.requestDevice(deviceDescriptor);
      if (!dev) {
        throw new Error("No appropriate GPUDevice found.");
      }
      setDevice(dev);
      setAdapter(adapt);
    })();
  }, [adapterOptions, deviceDescriptor]);
  if (!device || !adapter) {
    return null;
  }
  return (
    <DeviceContext.Provider value={{ device, adapter }}>
      {children}
    </DeviceContext.Provider>
  );
};
