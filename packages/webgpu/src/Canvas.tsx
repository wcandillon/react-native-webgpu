import type { ViewProps, LayoutChangeEvent } from "react-native";
import { View } from "react-native";
import {
  forwardRef,
  useEffect,
  useImperativeHandle,
  useRef,
  useState,
  useLayoutEffect,
  useCallback,
} from "react";
import type { RefObject } from "react";

import WebGPUNativeView from "./WebGPUViewNativeComponent";

let CONTEXT_COUNTER = 1;
function generateContextId() {
  return CONTEXT_COUNTER++;
}

declare global {
  // eslint-disable-next-line no-var
  var RNWebGPU: {
    gpu: GPU;
    fabric: boolean;
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
  getContext(contextName: "webgpu"): RNCanvasContext | null;
  getNativeSurface: () => NativeCanvas;
  whenReady: (callback: () => void) => void;
}

interface Size {
  width: number;
  height: number;
}

const useSizeFabric = (ref: RefObject<View>) => {
  const [size, setSize] = useState<null | Size>(null);
  useLayoutEffect(() => {
    if (!ref.current) {
      throw new Error("Canvas ref is null");
    }
    ref.current.measureInWindow((_x, _y, width, height) => {
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
      if (size === null) {
        setSize({ width, height });
      }
    },
    [size],
  );
  return { size, onLayout };
};

export const Canvas = forwardRef<CanvasRef, ViewProps>(({onLayout: _onLayout, ...props}, ref) => {
  const viewRef = useRef(null);
  const FABRIC = RNWebGPU.fabric;
  const useSize = FABRIC ? useSizeFabric : useSizePaper;
  const [contextId, _] = useState(() => generateContextId());
  const cb = useRef<() => void>();
  const { size, onLayout } = useSize(viewRef);

  useEffect(() => {
    if (size && cb.current) {
      cb.current();
    }
  }, [size]);
  useImperativeHandle(ref, () => ({
    getNativeSurface: () => {
      // TODO: implement
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
    getCallback: () => {
      return cb.current;
    },
    getContext(contextName: "webgpu"): RNCanvasContext | null {
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
    <View ref={viewRef} onLayout={onLayout} {...props}>
      <WebGPUNativeView
        style={{ flex: 1 }}
        contextId={contextId}
      />
    </View>
  );
});
