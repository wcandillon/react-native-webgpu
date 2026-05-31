import React from "react";
import { runOnUI } from "react-native-reanimated";

import { ReanimatedExample } from "./Reanimated";

export const UIThread = () => {
  return <ReanimatedExample run={runOnUI} />;
};
