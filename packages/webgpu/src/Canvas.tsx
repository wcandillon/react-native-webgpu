import React, { useImperativeHandle, useRef, useState } from "react";
import type { ViewProps } from "react-native";
import { PixelRatio, View } from "react-native";

import WebGPUNativeView from "./WebGPUViewNativeComponent";

let CONTEXT_COUNTER = 1;
function generateContextId() {
  return CONTEXT_COUNTER++;
}

declare global {
  var RNWebGPU: {
    gpu: GPU;
    fabric: boolean;
    getNativeSurface: (contextId: number) => NativeCanvas;
    MakeWebGPUCanvasContext: (
      contextId: number,
      width: number,
      height: number,
      pixelRatio: number,
    ) => RNCanvasContext;
    DecodeToUTF8: (buffer: NodeJS.ArrayBufferView | ArrayBuffer) => string;
    createImageBitmap: typeof createImageBitmap;
  };
}

type SurfacePointer = bigint;

export interface CanvasSize {
  width: number;
  height: number;
  clientWidth: number;
  clientHeight: number;
}

export type NativeCanvas = CanvasSize & {
  surface: SurfacePointer;
};

export type RNCanvasContext = GPUCanvasContext & {
  present: () => void;
};

export interface CanvasRef {
  getContextId: () => number;
  measureView: (canvasTarget: unknown) => CanvasSize;
  getContext(contextName: "webgpu"): RNCanvasContext | null;
  getNativeSurface: () => NativeCanvas;
}

interface CanvasProps extends ViewProps {
  transparent?: boolean;
  ref?: React.Ref<CanvasRef>;
}

function getViewSize(view: View): { width: number; height: number } {
  // let widthRes = 0, heightRes = 0;
  // view.measure((x, y, width, height, pageX, pageY) => {
  //   widthRes = width;
  //   heightRes = height;
  // });
  //
  // console.log(`Size: ${widthRes}x${heightRes}`);
  // return { width: widthRes, height: heightRes };
  // getBoundingClientRect became stable in RN 0.83
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  const viewAny = view as any;
  const size =
    "getBoundingClientRect" in viewAny
      ? viewAny.getBoundingClientRect()
      : viewAny.unstable_getBoundingClientRect();
  return size;
}

export const Canvas = ({ transparent, ref, ...props }: CanvasProps) => {
  const viewRef = useRef<View>(null);
  const [contextId, _] = useState(() => generateContextId());
  useImperativeHandle(ref, () => {
    return {
      getContextId: () => contextId,
      getNativeSurface: () => {
        return RNWebGPU.getNativeSurface(contextId);
      },
      measureView: (canvasTarget: unknown): CanvasSize => {
        if (!viewRef.current) {
          throw new Error("[WebGPU] Cannot get context before mount");
        }

        const sz = getViewSize(viewRef.current);
        const pixelRatio = PixelRatio.get();
        const res = {
          width: sz.width * pixelRatio,
          height: sz.height * pixelRatio,
          clientWidth: sz.width,
          clientHeight: sz.height,
        };
        if (canvasTarget) {
          const canvas = canvasTarget as NativeCanvas;
          canvas.width = res.width;
          canvas.height = res.height;
          canvas.clientWidth = res.clientWidth;
          canvas.clientHeight = res.clientHeight;
        }
        return res;
      },
      getContext(contextName: "webgpu"): RNCanvasContext | null {
        if (contextName !== "webgpu") {
          throw new Error(`[WebGPU] Unsupported context: ${contextName}`);
        }
        if (!viewRef.current) {
          throw new Error("[WebGPU] Cannot get context before mount");
        }

        const pixelRatio = PixelRatio.get();
        const sz = getViewSize(viewRef.current);

        return RNWebGPU.MakeWebGPUCanvasContext(
          contextId,
          sz.width,
          sz.height,
          pixelRatio,
        );
      },
    } satisfies CanvasRef;
  });

  const withNativeId = { ...props, nativeID: `webgpu-container-${contextId}` };
  return (
    <View collapsable={false} ref={viewRef} {...withNativeId}>
      <WebGPUNativeView
        style={{ flex: 1 }}
        contextId={contextId}
        transparent={!!transparent}
      />
    </View>
  );
};
