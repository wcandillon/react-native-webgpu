import React from "react";
import { scheduleOnUI } from "react-native-worklets";

import { ReanimatedExample } from "./Reanimated";

export const UIThread = () => {
  return (
    <ReanimatedExample
      run={(worklet) =>
        (...args) =>
          scheduleOnUI(worklet, ...args)
        }
    />
  );
};
