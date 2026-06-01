import React, { useMemo } from "react";
import { createWorkletRuntime, runOnRuntime } from "react-native-worklets";

import { ReanimatedExample } from "./Reanimated";

export const DedicatedThread = () => {
  const runtime = useMemo(
    () => createWorkletRuntime({ name: "WebGPUDedicatedRuntime" }),
    [],
  );
  return (
    <ReanimatedExample run={(worklet) => runOnRuntime(runtime, worklet)} />
  );
};
