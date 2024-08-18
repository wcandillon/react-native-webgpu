import type { ViewProps } from "react-native";
import { forwardRef, useEffect, useImperativeHandle, useState } from "react";

import WebGPUNativeView from "./WebGPUViewNativeComponent";
import WebGPUNativeModule from "./NativeWebGPUModule";

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
    MakeWebGPUCanvasContext: (nativeCanvas: NativeCanvas) => CanvasContext;
    DecodeToUTF8: (buffer: NodeJS.ArrayBufferView | ArrayBuffer) => string;
    createImageBitmap: typeof createImageBitmap;
  };
}

export interface NativeCanvas {
  surface: bigint;
  width: number;
  height: number;
  clientWidth: number;
  clientHeight: number;
}

global.__WebGPUContextRegistry = {};
const WebGPUContextRegistry = global.__WebGPUContextRegistry;

type CanvasContext = GPUCanvasContext & {
  present: () => void;
  getNativeSurface: () => NativeCanvas;
};

export interface CanvasRef {
  getContext(contextName: "webgpu"): CanvasContext | null;
}

export const Canvas = forwardRef<CanvasRef, ViewProps>((props, ref) => {
  const [contextId, _] = useState(() => generateContextId());

  useImperativeHandle(ref, () => ({
    getNativeSurface: () => {
      WebGPUNativeModule.createSurfaceContext(contextId);
      return WebGPUContextRegistry[contextId];
    },
    getContext(contextName: "webgpu"): CanvasContext | null {
      if (contextName !== "webgpu") {
        throw new Error(`[WebGPU] Unsupported context: ${contextName}`);
      }
      WebGPUNativeModule.createSurfaceContext(contextId);
      const nativeSurface = WebGPUContextRegistry[contextId];
      if (!nativeSurface) {
        return null;
      }
      const ctx = RNWebGPU.MakeWebGPUCanvasContext(nativeSurface);
      return ctx;
    },
  }));

  useEffect(() => {
    return () => {
      delete WebGPUContextRegistry[contextId];
    };
  }, [contextId]);

  return <WebGPUNativeView {...props} contextId={contextId} />;
});
