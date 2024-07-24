import { ViewProps } from "react-native";
import WebGPUNativeView from "./WebGPUViewNativeComponent";
import WebGPUNativeModule from "./WebGPUNativeModule";
import { WebGPUContextRegistry } from "./index";
import { forwardRef, useEffect, useImperativeHandle, useState } from 'react';

let CONTEXT_COUNTER = 0;
function generateContextId() {
  return CONTEXT_COUNTER++;
}

type CanvasContext = GPUCanvasContext & { present: () => void };

export interface WebGPUViewRef {
  getContext(contextName: string): CanvasContext | null;
}

export const WebGPUView = forwardRef<WebGPUViewRef, ViewProps>((props, ref) => {
  const [contextId, _] = useState(() => generateContextId());

  useImperativeHandle(ref, () => ({
    getContext: (contextName: string): CanvasContext | null => {
      if (contextName !== 'webgpu') {
        throw new Error('[WebGPU] Unsupported context');
      }
      WebGPUNativeModule.createSurfaceContext(contextId);
      return WebGPUContextRegistry[contextId] as CanvasContext ?? null;
    },
  }), [ref]);

  useEffect(() => {
    return () => {
      delete WebGPUContextRegistry[contextId];
    }
  }, [contextId]);

  return <WebGPUNativeView {...props} contextId={contextId} />;
});