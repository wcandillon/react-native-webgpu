import type { TurboModule } from "react-native";

export interface Spec extends TurboModule {
  install: () => boolean;
}
