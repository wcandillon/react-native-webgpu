import { codegenNativeComponent } from "react-native";
import type { ViewProps, Int32 } from "react-native";

interface NativeProps extends ViewProps {
  contextId: Int32;
  transparent: boolean;
}

// eslint-disable-next-line import/no-default-export
export default codegenNativeComponent<NativeProps>("WebGPUView");
