import type { ViewProps } from "react-native";

export interface RenderEffectViewProps extends ViewProps {
  // Reserved for tuning the effect strength later; the PoC ships one shader.
  intensity?: number;
}
