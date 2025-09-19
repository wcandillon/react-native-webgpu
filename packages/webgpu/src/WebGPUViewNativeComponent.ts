import { codegenNativeComponent } from "react-native";
import type { ViewProps } from "react-native";

interface NativeProps extends ViewProps {
  contextId: number;
  transparent: boolean;
}

// eslint-disable-next-line import/no-default-export
export default codegenNativeComponent<NativeProps>("WebGPUView");
