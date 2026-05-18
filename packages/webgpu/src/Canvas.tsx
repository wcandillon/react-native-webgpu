import React, { useImperativeHandle, useRef, useState } from "react";
import type { ViewProps } from "react-native";
import { View } from "react-native";

import type { CanvasRef, NativeCanvas, RNCanvasContext } from "./types";
import WebGPUNativeView from "./WebGPUViewNativeComponent";

export type { CanvasRef, NativeCanvas, RNCanvasContext };

let CONTEXT_COUNTER = 1;
function generateContextId() {
  return CONTEXT_COUNTER++;
}

interface CanvasProps extends ViewProps {
  transparent?: boolean;
  ref?: React.Ref<CanvasRef>;
}

export const Canvas = ({ transparent, ref, ...props }: CanvasProps) => {
  const viewRef = useRef(null);
  const [contextId, _] = useState(() => generateContextId());
  useImperativeHandle(ref, () => ({
    getContextId: () => contextId,
    getNativeSurface: () => {
      return RNWebGPU.getNativeSurface(contextId);
    },
    whenReady: () =>
      new Promise<void>((resolve) => {
        const check = () => {
          if (RNWebGPU.getNativeSurface(contextId).surface !== 0n) {
            resolve();
            return;
          }
          requestAnimationFrame(check);
        };
        check();
      }),
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
        transparent={!!transparent}
      />
    </View>
  );
};
