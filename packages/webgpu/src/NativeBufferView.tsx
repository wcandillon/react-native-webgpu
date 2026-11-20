import React, {
  useCallback,
  useEffect,
  useImperativeHandle,
  useRef,
  useState,
} from "react";
import type { NativeSyntheticEvent, ViewProps } from "react-native";

import NativeBufferViewNative from "./NativeBufferViewNativeComponent";
import type { GPUSharedFenceState, NativeVideoFrame } from "./types";

/**
 * How a {@link NativeBufferView} fits a buffer whose size differs from its
 * own (in physical pixels), for instance while the producer catches up with
 * a resize. A buffer of exactly the view's size is shown 1:1 in every mode.
 *
 * - `stretch` (the default): scaled to the bounds, ignoring the aspect ratio.
 * - `contain`: scaled to fit inside the bounds, keeping the aspect ratio.
 * - `cover`: scaled to fill the bounds, keeping the aspect ratio; the excess
 *   is cropped.
 * - `topLeft`: not scaled; anchored at the top left corner and cropped.
 */
export type NativeBufferResizeMode =
  "stretch" | "contain" | "cover" | "topLeft";

/** A size in physical pixels. */
export interface NativeBufferPixelSize {
  width: number;
  height: number;
}

export interface NativeBufferViewRef {
  /**
   * The id to pass to `RNWebGPU.presentNativeBuffer(viewId, frame, fence)`,
   * for instance from a worklet.
   */
  readonly viewId: number;
  /**
   * Displays `frame` once `fence` has signaled (right away without one).
   * Returns immediately; the view shows the buffer on the next vsync after
   * that. The view keeps its own reference on the buffer: the frame can be
   * released right after this call.
   *
   * The buffer must not be written again while it is displayed: render the
   * next content into another buffer and present that one.
   */
  present(frame: NativeVideoFrame, fence?: GPUSharedFenceState | null): void;
}

export interface NativeBufferViewProps extends ViewProps {
  /** See {@link NativeBufferResizeMode}. Defaults to `"stretch"`. */
  resizeMode?: NativeBufferResizeMode;
  /**
   * The view's size in physical pixels: the size to render at for a sharp,
   * 1:1 display. Called on mount and on every size change, with the
   * rounding the platform's layout applied. See {@link useNativeBufferSize}
   * to re-render only once a resize settles.
   */
  onPixelSizeChange?: (size: NativeBufferPixelSize) => void;
  ref?: React.Ref<NativeBufferViewRef>;
}

/**
 * Displays a native buffer (an `AHardwareBuffer` on Android, an `IOSurface`
 * on Apple platforms) with no swap chain, no canvas context and no GPU device
 * on the display side. Meant for content that is static or updated
 * occasionally; animated content belongs in a `Canvas`.
 *
 * The view never allocates buffers. It reports its size in physical pixels
 * ({@link NativeBufferViewProps.onPixelSizeChange}), and any producer
 * (WebGPU, Skia, a camera, a decoder) renders into a buffer and presents it
 * through the ref. With WebGPU:
 *
 * ```ts
 * const frame = RNWebGPU.createNativeBuffer(size.width, size.height);
 * const target = device.createNativeBufferTexture(frame, {
 *   usage: GPUTextureUsage.RENDER_ATTACHMENT,
 * });
 * target.beginAccess();
 * // ... render into target.texture and submit ...
 * const fence = target.endAccess();
 * ref.current.present(frame, fence);
 * target.destroy();
 * frame.release();
 * ```
 */
export const NativeBufferView = ({
  resizeMode,
  onPixelSizeChange,
  ref,
  ...props
}: NativeBufferViewProps) => {
  // Native hands out the id: unique for the life of the process, as the
  // native registry outlives a reload.
  const [viewId] = useState(() => RNWebGPU.allocateNativeBufferViewId());

  useImperativeHandle(
    ref,
    () => ({
      viewId,
      present: (frame, fence) =>
        RNWebGPU.presentNativeBuffer(viewId, frame, fence ?? null),
    }),
    [viewId],
  );

  useEffect(() => {
    return () => RNWebGPU.releaseNativeBufferView(viewId);
  }, [viewId]);

  const onNativePixelSizeChange = useCallback(
    (event: NativeSyntheticEvent<NativeBufferPixelSize>) => {
      const { width, height } = event.nativeEvent;
      onPixelSizeChange?.({ width, height });
    },
    [onPixelSizeChange],
  );

  return (
    <NativeBufferViewNative
      {...props}
      viewId={viewId}
      resizeMode={resizeMode}
      onPixelSizeChange={onNativePixelSizeChange}
    />
  );
};

/**
 * Tracks the pixel size of a {@link NativeBufferView}, settling resizes:
 * the first size is reported right away, later ones once the view has kept
 * the same size for `debounceMs` (during an animated resize the view
 * stretches what it shows meanwhile). Pass `onPixelSizeChange` to the view
 * and render a buffer of `size` whenever it changes.
 */
export const useNativeBufferSize = (debounceMs = 150) => {
  const [size, setSize] = useState<NativeBufferPixelSize | null>(null);
  const current = useRef<NativeBufferPixelSize | null>(null);
  const timer = useRef<ReturnType<typeof setTimeout> | null>(null);

  const onPixelSizeChange = useCallback(
    (next: NativeBufferPixelSize) => {
      if (timer.current !== null) {
        clearTimeout(timer.current);
        timer.current = null;
      }
      const settle = () => {
        timer.current = null;
        const previous = current.current;
        if (
          previous === null ||
          previous.width !== next.width ||
          previous.height !== next.height
        ) {
          current.current = next;
          setSize(next);
        }
      };
      if (current.current === null || debounceMs <= 0) {
        settle();
      } else {
        timer.current = setTimeout(settle, debounceMs);
      }
    },
    [debounceMs],
  );

  useEffect(
    () => () => {
      if (timer.current !== null) {
        clearTimeout(timer.current);
      }
    },
    [],
  );

  return { size, onPixelSizeChange };
};
