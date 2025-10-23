import type { Int32 } from "react-native/Libraries/Types/CodegenTypes";
import type { ViewProps } from "react-native";

export interface NativeProps extends ViewProps {
  contextId: Int32;
  transparent: boolean;
}
