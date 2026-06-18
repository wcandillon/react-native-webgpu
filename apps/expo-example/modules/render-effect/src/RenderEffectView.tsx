import { requireNativeViewManager } from "expo-modules-core";
import * as React from "react";

import type { RenderEffectViewProps } from "./RenderEffect.types";

const NativeView: React.ComponentType<RenderEffectViewProps> =
  requireNativeViewManager("RenderEffect");

export function RenderEffectView(props: RenderEffectViewProps) {
  return <NativeView {...props} />;
}
