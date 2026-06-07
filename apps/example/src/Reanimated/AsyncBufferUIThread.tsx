import React from "react";
import { runOnUI } from "react-native-reanimated";

import { AsyncBufferExample } from "./AsyncBuffer";

export const AsyncBufferUIThread = () => {
  return <AsyncBufferExample run={runOnUI} />;
};
