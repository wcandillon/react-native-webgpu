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

// Global registry for auto-present contexts
const autoPresentContexts = new Set<{
  context: RNCanvasContext;
  needsPresent: boolean;
}>();
let autoPresentLoopStarted = false;

// Start the global auto-present loop
function startAutoPresentLoop() {
  if (autoPresentLoopStarted) {
    return;
  }
  autoPresentLoopStarted = true;

  const autoPresentFrame = () => {
    // Present all contexts that need it
    autoPresentContexts.forEach((entry) => {
      if (entry.needsPresent) {
        entry.context.present();
        entry.needsPresent = false;
      }
    });

    if (autoPresentContexts.size > 0) {
      requestAnimationFrame(autoPresentFrame);
    } else {
      autoPresentLoopStarted = false;
    }
  };

  requestAnimationFrame(autoPresentFrame);
}

// Auto-present context interface
interface AutoPresentContext extends RNCanvasContext {
  _markNeedsPresent: () => void;
  _cleanup: () => void;
}

// Create an auto-present context wrapper
function createAutoPresentContext(
  baseContext: RNCanvasContext,
): AutoPresentContext {
  const contextEntry = {
    context: baseContext,
    needsPresent: false,
  };

  // Add to registry and start loop
  autoPresentContexts.add(contextEntry);
  startAutoPresentLoop();

  // Create wrapper context
  const autoPresentContext = {
    ...baseContext,
    present: () => {
      // In auto mode, just call the base present - the loop handles timing
      baseContext.present();
      contextEntry.needsPresent = false;
    },
    // Add a method to mark as needing present (can be called by hooks/utilities)
    _markNeedsPresent: () => {
      contextEntry.needsPresent = true;
    },
    // Cleanup method
    _cleanup: () => {
      autoPresentContexts.delete(contextEntry);
    },
  };

  return autoPresentContext;
}

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

export const Canvas = forwardRef<
  CanvasRef,
  ViewProps & { transparent?: boolean; autoPresent?: boolean }
>(
  (
    { onLayout: _onLayout, transparent, autoPresent = false, ...props },
    ref,
  ) => {
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
      getContextId: () => contextId,
      getNativeSurface: () => {
        if (size === null) {
          throw new Error("[WebGPU] Canvas size is not available yet");
        }
        return RNWebGPU.getNativeSurface(contextId);
      },
      whenReady(callback: () => void) {
        if (size === null) {
          cb.current = callback;
        } else {
          callback();
        }
      },
      getContext(contextName: "webgpu"): RNCanvasContext | null {
        if (contextName !== "webgpu") {
          throw new Error(`[WebGPU] Unsupported context: ${contextName}`);
        }
        if (size === null) {
          throw new Error("[WebGPU] Canvas size is not available yet");
        }
        const context = RNWebGPU.MakeWebGPUCanvasContext(
          contextId,
          size.width,
          size.height,
        );

        if (autoPresent) {
          return createAutoPresentContext(context);
        }

        return context;
      },
    }));
    return (
      <View collapsable={false} ref={viewRef} onLayout={onLayout} {...props}>
        <WebGPUNativeView
          style={size}
          contextId={contextId}
          transparent={!!transparent}
        />
      </View>
    );
  },
);
