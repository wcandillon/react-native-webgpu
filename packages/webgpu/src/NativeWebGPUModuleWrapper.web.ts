import type { TurboModule } from "react-native/Libraries/TurboModule/RCTExport";

interface Spec extends TurboModule {
  install: () => boolean;
  createSurfaceContext: (contextId: number) => boolean;
}

export const WebGPUModule: Spec = {
  createSurfaceContext: () => true,
  getConstants: () => ({}),
  install: () => true,
};
