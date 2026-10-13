import React from "react";
import { scheduleOnUI } from "react-native-worklets";

import { AsyncBufferExample } from "./AsyncBuffer";

export const AsyncBufferUIThread = () => {
  return (
    <AsyncBufferExample
      run={(worklet) =>
        (...args) =>
          scheduleOnUI(worklet, ...args)
        }
    />
  );
};
