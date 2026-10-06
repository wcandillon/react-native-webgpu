import React, { useEffect, useImperativeHandle, useRef, useState } from "react";
import type { ViewProps } from "react-native";
import { View } from "react-native";

import WebGPUNativeView from "./WebGPUViewNativeComponent";

let CONTEXT_COUNTER = 1;
function generateContextId() {
  return CONTEXT_COUNTER++;
}

type SurfacePointer = bigint;

export interface NativeCanvas {
  surface: SurfacePointer;
  width: number;
  height: number;
  clientWidth: number;
  clientHeight: number;
  // No-op DOM-compatibility stubs so web renderers (Three.js,
  // react-three-fiber) can treat the canvas like an HTMLCanvasElement.
  addEventListener(type: string, listener: EventListener): void;
  removeEventListener(type: string, listener: EventListener): void;
  dispatchEvent(event: Event): void;
  setPointerCapture(pointerId: number): void;
  releasePointerCapture(pointerId: number): void;
}

export type RNCanvasContext = GPUCanvasContext & {
  /**
   * Present the current frame.
   *
   * Call this after `queue.submit()` on every runtime: the main JS runtime, the
   * Reanimated UI runtime, and dedicated worklet runtimes (e.g.
   * `createWorkletRuntime` / `runOnRuntime`, or a Vision Camera frame
   * processor). It runs synchronously on the calling thread, so the frame is
   * presented from whichever thread did the rendering.
   */
  present: () => void;
};

export interface CanvasRef {
  getContextId: () => number;
  getContext(contextName: "webgpu"): RNCanvasContext | null;
  getNativeSurface: () => NativeCanvas;
}

export type AndroidSurfaceType =
  "SurfaceView" | "TextureView" | "HardwareBufferView";

export interface AndroidCanvasProps {
  /**
   * Backing view. Defaults to `SurfaceView` when the canvas is opaque and to
   * `TextureView` otherwise; both composite correctly in React Native
   * stacking order without further flags. `HardwareBufferView` (Android 10+,
   * a plain View that draws each frame's AHardwareBuffer inline with no extra
   * copy) is opt-in and experimental; on older devices it uses `TextureView`.
   */
  surfaceType?: AndroidSurfaceType;
  /**
   * SurfaceView only: composite above every React Native view in the window,
   * ignoring `zIndex`. Ignored for TextureView. Defaults to false.
   */
  zOrderOnTop?: boolean;
}

export type CanvasPresentation = "direct" | "copy";

export interface CanvasProps extends ViewProps {
  /**
   * Defaults to true. Set to false to alpha-composite the canvas over the
   * views behind it (pair it with `alphaMode: "premultiplied"` and an alpha-0
   * clear color). Android and web only; on iOS `alphaMode` alone controls it.
   */
  opaque?: boolean;
  /** Android-only rendering options. Ignored on iOS and web. */
  android?: AndroidCanvasProps;
  /**
   * Experimental. How finished frames reach the screen. `"direct"` (the
   * default) lets the thread that renders present the native swapchain.
   * `"copy"` renders into textures the canvas owns and has the UI thread copy
   * the latest one onto the native surface, at most once per display
   * refresh: one extra copy per frame, and the swapchain is only ever used
   * from the UI thread. On Android it selects a `TextureView` and takes
   * precedence over `android.surfaceType`. Ignored on macOS and web. Best set
   * when the canvas mounts: switching a mounted canvas to `"copy"` leaves it
   * empty until the next frame is rendered.
   */
  presentation?: CanvasPresentation;
  ref?: React.Ref<CanvasRef>;
}

// Anything else reaching the native component would hit the generated
// string-enum parser, which aborts on unknown values.
const resolveSurfaceType = (
  surfaceType: AndroidSurfaceType | undefined,
): "auto" | AndroidSurfaceType =>
  surfaceType === "SurfaceView" ||
  surfaceType === "TextureView" ||
  surfaceType === "HardwareBufferView"
    ? surfaceType
    : "auto";

export const Canvas = ({
  opaque = true,
  android,
  presentation,
  ref,
  ...props
}: CanvasProps) => {
  const viewRef = useRef(null);
  const [contextId, _] = useState(() => generateContextId());
  // Retire the native registry entry for this contextId on unmount. When a
  // native surface is still attached, this is a no-op and the native view's
  // own teardown retires the entry instead — which keeps StrictMode's
  // simulated unmount (which re-runs effects without unmounting native views)
  // from orphaning a live surface.
  useEffect(() => {
    return () => {
      RNWebGPU.destroyContext(contextId);
    };
  }, [contextId]);
  useImperativeHandle(ref, () => ({
    getContextId: () => contextId,
    getNativeSurface: () => {
      return RNWebGPU.getNativeSurface(contextId);
    },
    getContext(contextName: "webgpu"): RNCanvasContext | null {
      if (contextName !== "webgpu") {
        throw new Error(`[WebGPU] Unsupported context: ${contextName}`);
      }
      if (!viewRef.current) {
        throw new Error("[WebGPU] Cannot get context before mount");
      }
      // getBoundingClientRect became stable in RN 0.83
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      const view = viewRef.current as any;
      const size =
        "getBoundingClientRect" in view
          ? view.getBoundingClientRect()
          : view.unstable_getBoundingClientRect();
      return RNWebGPU.MakeWebGPUCanvasContext(
        contextId,
        size.width,
        size.height,
      );
    },
  }));

  return (
    <View collapsable={false} ref={viewRef} {...props}>
      <WebGPUNativeView
        style={{ flex: 1 }}
        contextId={contextId}
        opaque={opaque}
        androidSurfaceType={resolveSurfaceType(android?.surfaceType)}
        androidZOrderOnTop={!!android?.zOrderOnTop}
        presentation={presentation === "copy" ? "copy" : "direct"}
      />
    </View>
  );
};
