import React, { useMemo } from "react";
import { createWorkletRuntime, runOnRuntime } from "react-native-worklets";

import { AsyncBufferExample } from "./AsyncBuffer";

export const AsyncBufferDedicatedThread = () => {
  const runtime = useMemo(
    () => createWorkletRuntime({ name: "WebGPUAsyncBufferRuntime" }),
    [],
  );
  return (
    <AsyncBufferExample run={(worklet) => runOnRuntime(runtime, worklet)} />
  );
};
