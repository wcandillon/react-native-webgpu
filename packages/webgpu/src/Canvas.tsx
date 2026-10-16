import React, { useEffect, useImperativeHandle, useRef, useState } from "react";
import type { ViewProps } from "react-native";
import { Platform, View } from "react-native";

import type { CanvasRef, RNCanvasContext } from "./types";
import WebGPUNativeView from "./WebGPUViewNativeComponent";

export type { CanvasRef, NativeCanvas, RNCanvasContext } from "./types";

let CONTEXT_COUNTER = 1;
function generateContextId() {
  return CONTEXT_COUNTER++;
}

export type AndroidSurfaceType =
  "SurfaceView" | "TextureView" | "HardwareBufferView";

export interface AndroidCanvasProps {
  /**
   * Backing view. Defaults to `SurfaceView` when the canvas is opaque and to
   * `TextureView` otherwise; both composite correctly in React Native
   * stacking order without further flags. Before Android 12L an opaque canvas
   * defaults to `TextureView` too, unless it sets `zOrderOnTop`: those
   * releases remove a `SurfaceView` as it leaves the window, a frame before
   * the window stops showing it. `HardwareBufferView` (Android 10+, a plain
   * View that draws each frame's AHardwareBuffer inline with no extra copy) is
   * opt-in and experimental; on older devices it uses `TextureView`.
   */
  surfaceType?: AndroidSurfaceType;
  /**
   * SurfaceView only: composite above every React Native view in the window,
   * ignoring `zIndex`. Ignored for TextureView. Defaults to false.
   */
  zOrderOnTop?: boolean;
}

export interface CanvasProps extends ViewProps {
  /**
   * Defaults to true. Set to false to alpha-composite the canvas over the
   * views behind it (pair it with `alphaMode: "premultiplied"` and an alpha-0
   * clear color). Android and web only; on iOS `alphaMode` alone controls it.
   */
  opaque?: boolean;
  /** Android-only rendering options. Ignored on iOS and web. */
  android?: AndroidCanvasProps;
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

// A view measured relative to itself gets its layout size, without the
// transforms getBoundingClientRect() includes; measureLayout() calls back
// synchronously on the new architecture. On the web react-native-web measures
// asynchronously, so the DOM node is read directly: clientWidth/clientHeight
// are layout values that ignore transforms too (unlike getBoundingClientRect)
// and are what browser WebGPU code sizes a canvas from.
const measureLayout = (view: View) => {
  if (Platform.OS === "web") {
    const { clientWidth, clientHeight } = view as unknown as HTMLElement;
    return { width: clientWidth, height: clientHeight };
  }
  let size = { width: 0, height: 0 };
  view.measureLayout(view, (_left, _top, width, height) => {
    size = { width, height };
  });
  return size;
};

export const Canvas = ({
  opaque = true,
  android,
  ref,
  ...props
}: CanvasProps) => {
  const viewRef = useRef<View>(null);
  const [contextId, _] = useState(() => generateContextId());
  // Retire the native registry entry for this contextId on unmount. When a
  // native surface is still attached, this is a no-op and the native view's
  // own teardown retires the entry instead — which keeps StrictMode's
  // simulated unmount (which re-runs effects without unmounting native views)
  // from orphaning a live surface. When no surface is attached yet (StrictMode
  // runs this cleanup right after mount, possibly before the UI thread got to
  // it), a context kept from the first effect run keeps its surface entry,
  // which the native view takes over when it attaches.
  useEffect(() => {
    return () => {
      RNWebGPU.destroyContext(contextId);
    };
  }, [contextId]);
  useImperativeHandle(
    ref,
    () => ({
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
        const size = measureLayout(viewRef.current);
        return RNWebGPU.MakeWebGPUCanvasContext(
          contextId,
          size.width,
          size.height,
        );
      },
    }),
    [contextId],
  );

  return (
    <View collapsable={false} ref={viewRef} {...props}>
      <WebGPUNativeView
        style={{ flex: 1 }}
        contextId={contextId}
        opaque={opaque}
        androidSurfaceType={resolveSurfaceType(android?.surfaceType)}
        androidZOrderOnTop={!!android?.zOrderOnTop}
      />
    </View>
  );
};
