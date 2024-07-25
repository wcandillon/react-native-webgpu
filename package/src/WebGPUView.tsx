import type { ViewProps } from "react-native";
import { forwardRef, useEffect, useImperativeHandle, useState } from "react";

import WebGPUNativeView from "./WebGPUViewNativeComponent";
import WebGPUNativeModule from "./WebGPUNativeModule";

let CONTEXT_COUNTER = 0;
function generateContextId() {
  return CONTEXT_COUNTER++;
}

global.__WebGPUContextRegistry = {};
console.log(global.__WebGPUContextRegistry);
const WebGPUContextRegistry = global.__WebGPUContextRegistry;

type CanvasContext = GPUCanvasContext & {
  present: () => void;
};

export interface WebGPUViewRef {
  getContext(contextName: "webgpu"): CanvasContext | null;
}

export const WebGPUView = forwardRef<WebGPUViewRef, ViewProps>((props, ref) => {
  const [contextId, _] = useState(() => generateContextId());

  useImperativeHandle(ref, () => ({
    getContext(contextName: "webgpu"): CanvasContext | null {
      if (contextName !== "webgpu") {
        throw new Error("[WebGPU] Unsupported context");
      }
      WebGPUNativeModule.createSurfaceContext(contextId);
      const ctx = (WebGPUContextRegistry[contextId] as CanvasContext) ?? null;
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
