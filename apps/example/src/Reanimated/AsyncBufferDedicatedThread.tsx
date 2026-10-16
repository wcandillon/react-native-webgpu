import React, { useMemo } from "react";
import { createWorkletRuntime, scheduleOnRuntime } from "react-native-worklets";

import { AsyncBufferExample } from "./AsyncBuffer";

export const AsyncBufferDedicatedThread = () => {
  const runtime = useMemo(
    () => createWorkletRuntime({ name: "WebGPUAsyncBufferRuntime" }),
    [],
  );
  return (
    <AsyncBufferExample
      run={(worklet) =>
        (...args) =>
          scheduleOnRuntime(runtime, worklet, ...args)
        }
    />
  );
};
