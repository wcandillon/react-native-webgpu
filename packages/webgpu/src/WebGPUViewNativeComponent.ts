import { codegenNativeComponent } from "react-native";
import type {
  Int32,
  WithDefault,
} from "react-native/Libraries/Types/CodegenTypes";
import type { ViewProps } from "react-native";

export interface NativeProps extends ViewProps {
  contextId: Int32;
  opaque?: WithDefault<boolean, true>;
  androidSurfaceType?: WithDefault<
    "auto" | "SurfaceView" | "TextureView",
    "auto"
  >;
  androidZOrderOnTop?: WithDefault<boolean, false>;
}

// eslint-disable-next-line import/no-default-export
export default codegenNativeComponent<NativeProps>("WebGPUView");
