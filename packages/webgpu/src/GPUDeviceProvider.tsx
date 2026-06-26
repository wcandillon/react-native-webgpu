import React, { createContext, useContext } from "react";
import type { ReactNode } from "react";

import { useDevice } from "./hooks";

interface DeviceContextValue {
  device: GPUDevice | null;
  adapter: GPUAdapter | null;
}

const DeviceContext = createContext<DeviceContextValue | null>(null);

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

export const useMainDevice = () => {
  const ctx = useContext(DeviceContext);
  if (!ctx) {
    throw new Error("No DeviceContext found.");
  }
  return ctx;
};
