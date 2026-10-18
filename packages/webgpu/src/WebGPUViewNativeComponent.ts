import { codegenNativeComponent } from "react-native";
import type {
  Int32,
  WithDefault,
} from "react-native/Libraries/Types/CodegenTypes";
import type { ViewProps } from "react-native";

export interface NativeProps extends ViewProps {
  contextId: Int32;
  // The `android` prop, resolved (see Canvas.tsx)
  androidBackingView?: WithDefault<
    "TextureView" | "SurfaceView" | "HardwareBufferView",
    "TextureView"
  >;
  androidOpaque?: WithDefault<boolean, false>;
  androidZOrderOnTop?: WithDefault<boolean, false>;
  androidZOrderMediaOverlay?: WithDefault<boolean, false>;
}

// eslint-disable-next-line import/no-default-export
export default codegenNativeComponent<NativeProps>("WebGPUView");
