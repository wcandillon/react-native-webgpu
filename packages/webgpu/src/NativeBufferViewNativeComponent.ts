import { codegenNativeComponent } from "react-native";
import type { CodegenTypes, ViewProps } from "react-native";

type PixelSizeChangeEvent = Readonly<{
  width: CodegenTypes.Int32;
  height: CodegenTypes.Int32;
}>;

export interface NativeProps extends ViewProps {
  // The NativeBufferViewRegistry entry (see NativeBufferView.tsx).
  viewId: CodegenTypes.Int32;
  resizeMode?: CodegenTypes.WithDefault<
    "stretch" | "contain" | "cover" | "topLeft",
    "stretch"
  >;
  onPixelSizeChange?: CodegenTypes.DirectEventHandler<PixelSizeChangeEvent>;
}

// eslint-disable-next-line import/no-default-export
export default codegenNativeComponent<NativeProps>("WebGPUNativeBufferView");
