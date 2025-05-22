import type { ViewProps, LayoutChangeEvent } from "react-native";
import { View } from "react-native";
import {
  forwardRef,
  useImperativeHandle,
  useRef,
  useState,
  useLayoutEffect,
  useCallback,
} from "react";
import type { RefObject } from "react";

import WebGPUNativeView from "./WebGPUViewNativeComponent";
import { useSignal } from "./useSignal";

let CONTEXT_COUNTER = 1;
function generateContextId() {
  return CONTEXT_COUNTER++;
}

declare global {
  // eslint-disable-next-line no-var
  var RNWebGPU: {
    gpu: GPU;
    fabric: boolean;
    getNativeSurface: (contextId: number) => NativeCanvas;
    MakeWebGPUCanvasContext: (
      contextId: number,
      width: number,
      height: number,
    ) => RNCanvasContext;
    DecodeToUTF8: (buffer: NodeJS.ArrayBufferView | ArrayBuffer) => string;
    createImageBitmap: typeof createImageBitmap;
  };
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
  present: () => void;
};

export interface CanvasRef {
  getContextId: () => number;
  getContext(contextName: "webgpu"): RNCanvasContext | null;
  getNativeSurface: () => NativeCanvas;
  whenReady: (callback: () => void) => void;
}

interface Size {
  width: number;
  height: number;
}

const useSizeFabric = (ref: RefObject<View>, onSizeChange: (v: Size) => void) => {
  const [sizeSignal] = useSignal<null | Size>(null);
  useLayoutEffect(() => {
    if (!ref.current) {
      throw new Error("Canvas ref is null");
    }
    ref.current.measureInWindow((_x, _y, width, height) => {
      const size: Size = { width, height };
      sizeSignal.set(size);
      onSizeChange(size);
    });
  }, [ref]);
  return { sizeSignal, onLayout: undefined };
};

const useSizePaper = (_ref: RefObject<View>, onSizeChange: (v: Size) => void) => {
  const [sizeSignal] = useSignal<null | Size>(null);
  const onLayout = useCallback<(event: LayoutChangeEvent) => void>(
    ({
      nativeEvent: {
        layout: { width, height },
      },
    }) => {
      if (sizeSignal.get() === null) {
        const size: Size = { width, height };
        sizeSignal.set(size);
        onSizeChange(size);
      }
    },
    [sizeSignal],
  );
  return { sizeSignal, onLayout };
};

export const Canvas = forwardRef<
  CanvasRef,
  ViewProps & { transparent?: boolean }
>(({ onLayout: _onLayout, transparent, ...props }, ref) => {
  const viewRef = useRef(null);
  const FABRIC = RNWebGPU.fabric;
  const useSize = FABRIC ? useSizeFabric : useSizePaper;
  const [contextId, _] = useState(generateContextId);
  const whenReadyCallbacks = useRef<(() => void)[]>([]);
  const onSizeChange = useCallback(() => {
    // The size of the canvas has been computed, meaning we're ready
    // to display things on it!
    whenReadyCallbacks.current.forEach(cb => cb());
    whenReadyCallbacks.current = [];
  }, []);
  const { sizeSignal, onLayout } = useSize(viewRef, onSizeChange);
  
  useImperativeHandle(ref, () => ({
    getContextId: () => contextId,
    getNativeSurface: () => {
      if (sizeSignal.get() === null) {
        throw new Error("[WebGPU] Canvas size is not available yet");
      }
      return RNWebGPU.getNativeSurface(contextId);
    },
    whenReady(callback: () => void) {
      if (sizeSignal.get() === null) {
        whenReadyCallbacks.current.push(callback);
      } else {
        callback();
      }
    },
    getContext(contextName: "webgpu"): RNCanvasContext | null {
      const size = sizeSignal.get();
      if (contextName !== "webgpu") {
        throw new Error(`[WebGPU] Unsupported context: ${contextName}`);
      }
      if (size === null) {
        throw new Error("[WebGPU] Canvas size is not available yet");
      }
      return RNWebGPU.MakeWebGPUCanvasContext(
        contextId,
        size.width,
        size.height,
      );
    },
  }));
  return (
    <View collapsable={false} ref={viewRef} onLayout={onLayout} {...props}>
      <WebGPUNativeView
        style={{ flex: 1 }}
        contextId={contextId}
        transparent={!!transparent}
      />
    </View>
  );
});
