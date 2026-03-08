import { TurboModuleRegistry } from "react-native";
import type { TurboModule } from "react-native";

export interface Spec extends TurboModule {
  install: (options?: { [key: string]: unknown } | null) => boolean;
}

// eslint-disable-next-line import/no-default-export
export default TurboModuleRegistry.getEnforcing<Spec>("WebGPUModule");
