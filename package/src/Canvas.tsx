import type { ViewProps } from "react-native";
import { forwardRef, useEffect, useImperativeHandle, useState } from "react";

import WebGPUNativeView from "./WebGPUViewNativeComponent";
import WebGPUNativeModule from "./NativeWebGPUModule";

let CONTEXT_COUNTER = 1;
function generateContextId() {
  return CONTEXT_COUNTER++;
}

global.__WebGPUContextRegistry = {};
const WebGPUContextRegistry = global.__WebGPUContextRegistry;

type CanvasContext = GPUCanvasContext & {
  present: () => void;
};

export interface CanvasRef {
  getContext(contextName: "webgpu"): CanvasContext | null;
}

export const Canvas = forwardRef<CanvasRef, ViewProps>((props, ref) => {
  const [contextId, _] = useState(() => generateContextId());

  useImperativeHandle(ref, () => ({
    getContext(contextName: "webgpu"): CanvasContext | null {
      // if (contextName !== "webgpu") {
      //   throw new Error("[WebGPU] Unsupported context");
      // }
      // WebGPUNativeModule.createSurfaceContext(contextId);
      // console.log('mleko')
      // console.log('WebGPUNativeModule', WebGPUNativeModule)
      WebGPUNativeModule.createSurfaceContext(contextId);
      // console.log('WebGPUNativeModule', WebGPUNativeModule)
      // console.log('getcontext')
      const ctx = (WebGPUContextRegistry[contextId] as CanvasContext) ?? null;
      return ctx;
    },
  }));

  useEffect(() => {
    // console.log('useEffect')
    return () => {
      delete WebGPUContextRegistry[contextId];
    };
  }, [contextId]);

  return <WebGPUNativeView {...props} contextId={contextId} />;
});
