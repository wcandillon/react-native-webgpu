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

/** The Android view a canvas draws into, see {@link AndroidCanvasProps}. */
export type AndroidBackingView =
  "TextureView" | "SurfaceView" | "HardwareBufferView";

/** The options of a canvas backed by a `TextureView`, the default. */
export interface AndroidTextureViewProps {
  /**
   * A `TextureView`: a regular view, composited like any other (parent
   * transforms, clipping, alpha and z-order apply), at the price of an extra
   * texture copy and typically a frame of latency. The default.
   */
  backingView?: "TextureView";
  /**
   * Declares that the canvas covers every pixel of its bounds, so the UI
   * toolkit skips blending the view. Defaults to false. Applied in place.
   */
  opaque?: boolean;
}

/** The options of a canvas backed by a `SurfaceView`. */
export interface AndroidSurfaceViewProps {
  /**
   * A `SurfaceView`: each frame goes to the system compositor as its own
   * layer, the fastest path, with no extra copy. It is not view content:
   * parent transforms, clipping, rounded corners and z-order with sibling
   * views do not apply to it.
   */
  backingView: "SurfaceView";
  /**
   * Declares that the canvas covers every pixel of its bounds, so the
   * compositor skips blending the layer and can show it through a hardware
   * overlay. Defaults to true: a translucent `SurfaceView` blends against
   * what is below the window, usually black, rather than the React Native
   * views beneath it (pair it with `zOrderOnTop`). Applied in place.
   */
  opaque?: boolean;
  /**
   * Composite above every React Native view in the window, ignoring
   * `zIndex`. Defaults to false. Applied in place on Android 11 and later;
   * read once, at mount, before that.
   */
  zOrderOnTop?: boolean;
  /**
   * Composite above the other `SurfaceView`s below the window (video players,
   * other canvases), while staying below the window itself. Defaults to
   * false; `zOrderOnTop` takes precedence. Applied in place on Android 11 and
   * later; read once, at mount, before that.
   */
  zOrderMediaOverlay?: boolean;
}

/** The options of a canvas backed by a `HardwareBufferView`. */
export interface AndroidHardwareBufferViewProps {
  /**
   * A plain view drawing each frame's `AHardwareBuffer` inline: composited
   * like a `TextureView`, without its extra copy. Opt-in and experimental,
   * on Android 10 and later; below that the canvas uses a `TextureView`,
   * with a warning in development. Always composites with alpha.
   */
  backingView: "HardwareBufferView";
}

/**
 * Android-only rendering options: the native view behind the canvas and its
 * options. The view is created when the canvas mounts and kept: a change of
 * `backingView` is ignored, with a warning in development; remount the canvas
 * (with a `key`) to change it. `opaque` applies in place, and so do the
 * z-order options on Android 11 and later. Ignored on iOS and web.
 */
export type AndroidCanvasProps =
  | AndroidTextureViewProps
  | AndroidSurfaceViewProps
  | AndroidHardwareBufferViewProps;

export interface CanvasProps extends ViewProps {
  /**
   * Android-only rendering options: the native view behind the canvas and its
   * options, see {@link AndroidCanvasProps}. Ignored on iOS and web.
   */
  android?: AndroidCanvasProps;
  ref?: React.Ref<CanvasRef>;
}

// The union read as one bag of options; which of them apply to the view is
// decided below, as it is on the native side.
type AndroidOptions = Partial<{
  backingView: AndroidBackingView;
  opaque: boolean;
  zOrderOnTop: boolean;
  zOrderMediaOverlay: boolean;
}>;

const options = (android?: AndroidCanvasProps): AndroidOptions =>
  (android ?? {}) as AndroidOptions;

// Anything else reaching the native component would hit the generated
// string-enum parser, which aborts on unknown values.
const resolveBackingView = (
  android?: AndroidCanvasProps,
): AndroidBackingView => {
  const { backingView } = options(android);
  return backingView === "SurfaceView" || backingView === "HardwareBufferView"
    ? backingView
    : "TextureView";
};

// Whether the canvas composites as opaque, see BackingViewKind.resolveOpaque():
// a SurfaceView is opaque unless asked otherwise, a TextureView is translucent
// unless asked otherwise, and a HardwareBufferView always composites with
// alpha.
const resolveOpaque = (android?: AndroidCanvasProps): boolean => {
  const { opaque } = options(android);
  switch (resolveBackingView(android)) {
    case "SurfaceView":
      return opaque !== false;
    case "TextureView":
      return opaque === true;
    case "HardwareBufferView":
      return false;
  }
};

// The native props the `android` prop resolves to.
const androidNativeProps = (android?: AndroidCanvasProps) => {
  const backingView = resolveBackingView(android);
  const { zOrderOnTop, zOrderMediaOverlay } = options(android);
  const surfaceView = backingView === "SurfaceView";
  return {
    androidBackingView: backingView,
    androidOpaque: resolveOpaque(android),
    androidZOrderOnTop: surfaceView && !!zOrderOnTop,
    androidZOrderMediaOverlay: surfaceView && !!zOrderMediaOverlay,
  };
};

const warnIgnoredChange = (prop: string) =>
  console.warn(
    `Ignoring a change of ${prop} on a mounted canvas: the backing view is ` +
      "chosen when the canvas mounts. Remount the canvas (give it a new key) " +
      "to change it.",
  );

// Warns, in development on Android, about an option a mounting canvas asks
// for that the native view cannot honour, and about a change of these props
// that the native view ignores: the backing view is created from them when
// the canvas mounts (see WebGPUView.java, whose setters these rules mirror).
// The native side only says so in logcat, which is not where a JS developer
// looks.
const useBackingViewWarning = (android?: AndroidCanvasProps) => {
  const mount = useRef(android);
  const prev = useRef(android);
  useEffect(() => {
    if (
      __DEV__ &&
      Platform.OS === "android" &&
      resolveBackingView(mount.current) === "HardwareBufferView" &&
      Number(Platform.Version) < 29
    ) {
      console.warn(
        "android.backingView: HardwareBufferView needs Android 10, the " +
          "canvas uses a TextureView.",
      );
    }
  }, []);
  useEffect(() => {
    if (__DEV__ && Platform.OS === "android") {
      const { current: before } = prev;
      if (resolveBackingView(before) !== resolveBackingView(android)) {
        warnIgnoredChange("android.backingView");
      }
      // Only a SurfaceView has a z-order of its own, which Android 11 and
      // later changes in place.
      const zOrderAtMount =
        resolveBackingView(mount.current) === "SurfaceView" &&
        Number(Platform.Version) < 30;
      const was = options(before);
      const is = options(android);
      if (zOrderAtMount && !!was.zOrderOnTop !== !!is.zOrderOnTop) {
        warnIgnoredChange("android.zOrderOnTop");
      }
      if (
        zOrderAtMount &&
        !!was.zOrderMediaOverlay !== !!is.zOrderMediaOverlay
      ) {
        warnIgnoredChange("android.zOrderMediaOverlay");
      }
    }
    prev.current = android;
  });
};

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

export const Canvas = ({ android, ref, ...props }: CanvasProps) => {
  const viewRef = useRef<View>(null);
  const [contextId, _] = useState(() => generateContextId());
  useBackingViewWarning(android);
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
        {...androidNativeProps(android)}
      />
    </View>
  );
};
