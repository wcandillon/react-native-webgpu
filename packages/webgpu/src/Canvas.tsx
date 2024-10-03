import type { ViewProps, LayoutChangeEvent, View } from "react-native";
import {
  forwardRef,
  useEffect,
  useImperativeHandle,
  useRef,
  useState,
  useLayoutEffect,
  useCallback,
} from "react";
import type { ForwardedRef, RefObject } from "react";

import WebGPUNativeView from "./WebGPUViewNativeComponent";
import WebGPUNativeModule from "./NativeWebGPUModule";
import { GPUOffscreenCanvas } from "./Offscreen";

let CONTEXT_COUNTER = 1;
function generateContextId() {
  return CONTEXT_COUNTER++;
}

declare global {
  // eslint-disable-next-line no-var
  var __WebGPUContextRegistry: Record<number, NativeCanvas>;
  // eslint-disable-next-line no-var
  var RNWebGPU: {
    gpu: GPU;
    MakeWebGPUCanvasContext: (nativeCanvas: NativeCanvas) => RNCanvasContext;
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

global.__WebGPUContextRegistry = {};
const WebGPUContextRegistry = global.__WebGPUContextRegistry;

export type RNCanvasContext = GPUCanvasContext & {
  present: () => void;
};

export interface CanvasRef {
  getContext(contextName: "webgpu"): RNCanvasContext | null;
  getNativeSurface: () => NativeCanvas;
  whenReady: (callback: () => void) => void;
}

// TODO: use JSI fucntion for that;
const FABRIC = false;

interface Size {
  width: number;
  height: number;
}

const useSizeFabric = (ref: RefObject<View>) => {
  const [size, setSize] = useState<null | Size>(null);
  useLayoutEffect(() => {
    ref.current?.measureInWindow((_x, _y, width, height) => {
      setSize({ width, height });
    });
  }, [ref]);
  return { size, onLayout: undefined };
};

const useSizePaper = (_ref: RefObject<View>) => {
  const [size, setSize] = useState<null | Size>(null);
  const onLayout = useCallback<(event: LayoutChangeEvent) => void>(
    ({
      nativeEvent: {
        layout: { width, height },
      },
    }) => {
      setSize({ width, height });
    },
    [],
  );
  return { size, onLayout };
};

const useSize = FABRIC ? useSizeFabric : useSizePaper;

export const Canvas = forwardRef<CanvasRef, ViewProps>((props, ref) => {
  const [contextId, _] = useState(() => generateContextId());
  const cb = useRef<() => void>();
  const { size, onLayout } = useSize(ref as RefObject<View>);
  useImperativeHandle(ref, () => ({
    getNativeSurface: () => {
      return {
        surface: 0n,
        width: 0,
        height: 0,
        clientWidth: 0,
        clientHeight: 0,
      };
    },
    whenReady(callback: () => void) {
      cb.current = callback;
    },
    getContext(contextName: "webgpu"): RNCanvasContext | null {
      if (contextName !== "webgpu") {
        throw new Error(`[WebGPU] Unsupported context: ${contextName}`);
      }
      if (size === null) {
        throw new Error("[WebGPU] Canvas size is not available yet");
      }
      const canvas = new GPUOffscreenCanvas(size.width, size.height);
      return canvas.getContext("webgpu");
    },
  }));

  useEffect(() => {
    return () => {
      delete WebGPUContextRegistry[contextId];
    };
  }, [contextId]);

  return (
    <WebGPUNativeView {...props} onLayout={onLayout} contextId={contextId} />
  );
});
