import { ViewProps } from "react-native";
import WebGPUNativeView from "./WebGPUViewNativeComponent";
import WebGPUNativeModule from "./WebGPUNativeModule";
import { WebGPUContextRegistry } from "./index";
import { forwardRef, useEffect, useImperativeHandle, useRef, useState } from 'react';

let CONTEXT_COUNTER = 0;
function generateContextId() {
  return CONTEXT_COUNTER++;
}

export interface WebGPUViewRef {
  getContext(contextName: string): GPUCanvasContext | null;
}

export const WebGPUView = forwardRef<WebGPUViewRef, ViewProps>((props, ref) => {

  const innerRef = useRef(null);
  const [contextId, _] = useState(() => generateContextId());

  useImperativeHandle(ref, () => ({
    getContext: (contextName: string): GPUCanvasContext | null => {
      if (contextName !== 'webgpu') {
        throw new Error('[WebGPU] Unsupported context');
      }
      return WebGPUContextRegistry[contextId] ?? null;
    },
  }), [ref]);

  useEffect(() => {
    WebGPUNativeModule.registerContext(contextId);
    return () => {
      delete WebGPUContextRegistry[contextId];
    }
  }, [contextId]);

  return <WebGPUNativeView {...props} ref={innerRef} contextId={contextId} />;
});