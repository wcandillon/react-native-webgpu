import React, { useImperativeHandle, useRef, useState } from "react";
import type { ViewProps } from "react-native";
import { StyleSheet, View } from "react-native";

import WebGPUNativeView from "./WebGPUViewNativeComponent";
import type { PaintEvent } from "./types";

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

interface CanvasProps extends ViewProps {
  transparent?: boolean;
  ref?: React.Ref<CanvasRef>;
  /**
   * "HTML in Canvas" opt-in. When set, the Canvas's child native views
   * participate in layout (so they can be painted into a GPUTexture via
   * `queue.copyElementImageToTexture`) but the canvas surface paints over them.
   * v1 is paint-only: children do not yet receive touches or accessibility.
   */
  layoutSubtree?: boolean;
  /**
   * Dispatched when the rendering of a layoutSubtree child changes. Reserved:
   * v1 re-renders on demand and does not emit this event yet.
   */
  onPaint?: (event: PaintEvent) => void;
}

export const Canvas = ({
  transparent,
  ref,
  layoutSubtree,
  // onPaint is reserved for the damage-tracked follow-up; accepted but unused.
  onPaint: _onPaint,
  children,
  ...props
}: CanvasProps) => {
  const viewRef = useRef(null);
  const [contextId, _] = useState(() => generateContextId());
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
      {layoutSubtree ? (
        <>
          {/* Children stay mounted, laid out, and interactive so they can be
              captured by their native tag. They sit behind the canvas, which
              paints the captured copy on top. The canvas is wrapped in a
              pointerEvents="none" view so touches fall through to the real
              children below: a 1:1 hit-test for this direct overlay. A
              transformed/3D placement would need explicit hit-test mapping. */}
          <View style={StyleSheet.absoluteFill}>{children}</View>
          <View style={StyleSheet.absoluteFill} pointerEvents="none">
            <WebGPUNativeView
              style={StyleSheet.absoluteFill}
              contextId={contextId}
              transparent={!!transparent}
            />
          </View>
        </>
      ) : (
        <>
          <WebGPUNativeView
            style={{ flex: 1 }}
            contextId={contextId}
            transparent={!!transparent}
          />
          {children}
        </>
      )}
    </View>
  );
};
