import type { ViewProps } from "react-native";
import { forwardRef, useEffect, useImperativeHandle, useState } from "react";

import WebGPUNativeView from "./WebGPUViewNativeComponent";
import WebGPUNativeModule from "./WebGPUNativeModule";

let CONTEXT_COUNTER = 0;
function generateContextId() {
  return CONTEXT_COUNTER++;
}

global.__WebGPUContextRegistry = {};
const WebGPUContextRegistry = global.__WebGPUContextRegistry;

type CanvasContext = GPUCanvasContext & { 
  present: () => void,
  width: number,
  height: number,
  clientWidth: number,
  clientHeight: number,
};

export interface WebGPUViewRef {
  getContext(contextName: string): CanvasContext | null;
}

export const WebGPUView = forwardRef<WebGPUViewRef, ViewProps>((props, ref) => {
  const [contextId, _] = useState(() => generateContextId());

  useImperativeHandle(ref, () => ({
    getContext: (contextName: string): CanvasContext | null => {
      if (contextName !== "webgpu") {
        throw new Error("[WebGPU] Unsupported context");
      }
      WebGPUNativeModule.createSurfaceContext(contextId);
      return (WebGPUContextRegistry[contextId] as CanvasContext) ?? null;
    },
  }));

  useEffect(() => {
    return () => {
      delete WebGPUContextRegistry[contextId];
    };
  }, [contextId]);

  return <WebGPUNativeView {...props} contextId={contextId} />;
});
