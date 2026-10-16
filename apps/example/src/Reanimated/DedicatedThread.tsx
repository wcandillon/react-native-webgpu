import React, { useMemo } from "react";
import { createWorkletRuntime, scheduleOnRuntime } from "react-native-worklets";

import { ReanimatedExample } from "./Reanimated";

export const DedicatedThread = () => {
  const runtime = useMemo(
    () => createWorkletRuntime({ name: "WebGPUDedicatedRuntime" }),
    [],
  );
  return (
    <ReanimatedExample
      run={(worklet) =>
        (...args) =>
          scheduleOnRuntime(runtime, worklet, ...args)
        }
    />
  );
};
