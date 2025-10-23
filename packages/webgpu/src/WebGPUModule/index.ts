import { TurboModuleRegistry } from "react-native";

import type { Spec } from "./types";

// eslint-disable-next-line import/no-default-export
export default TurboModuleRegistry.getEnforcing<Spec>("WebGPUModule");
