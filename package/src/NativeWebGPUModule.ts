import type { TurboModule } from "react-native/Libraries/TurboModule/RCTExport";
import { TurboModuleRegistry } from "react-native";

export interface Spec extends TurboModule {
  install: () => boolean;
  createSurfaceContext: (contextId: number) => boolean;
  createSurfaceContextAsync: (contextId: number) => Promise<boolean>;
}

// eslint-disable-next-line import/no-default-export
export default TurboModuleRegistry.getEnforcing<Spec>("WebGPUModule");
