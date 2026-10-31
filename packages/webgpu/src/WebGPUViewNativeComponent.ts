import { codegenNativeComponent } from "react-native";
import type { CodegenTypes, ViewProps } from "react-native";

export interface NativeProps extends ViewProps {
  contextId: CodegenTypes.Int32;
  // The `android` prop, resolved (see Canvas.tsx)
  androidBackingView?: CodegenTypes.WithDefault<
    "TextureView" | "SurfaceView" | "HardwareBufferView",
    "TextureView"
  >;
  androidOpaque?: CodegenTypes.WithDefault<boolean, false>;
  androidZOrderOnTop?: CodegenTypes.WithDefault<boolean, false>;
  androidZOrderMediaOverlay?: CodegenTypes.WithDefault<boolean, false>;
}

// eslint-disable-next-line import/no-default-export
export default codegenNativeComponent<NativeProps>("WebGPUView");
