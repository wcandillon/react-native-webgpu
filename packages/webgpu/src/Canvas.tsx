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

import { WebGPUWrapper } from "./WebGPUWrapper";
import type { CanvasRef, RNCanvasContext } from "./types";
import {
  fabricIsEnabled,
  getNativeSurface,
  MakeWebGPUCanvasContext,
} from "./utils";

let CONTEXT_COUNTER = 1;
function generateContextId() {
  return CONTEXT_COUNTER++;
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
  ViewProps & { transparent?: boolean }
>(({ onLayout: _onLayout, transparent, ...props }, ref) => {
  const viewRef = useRef(null);
  const useSize = fabricIsEnabled() ? useSizeFabric : useSizePaper;
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
      return getNativeSurface(contextId);
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
      return MakeWebGPUCanvasContext(contextId, size.width, size.height);
    },
  }));
  return (
    <View collapsable={false} ref={viewRef} onLayout={onLayout} {...props}>
      <WebGPUWrapper
        style={{ flex: 1 }}
        contextId={contextId}
        transparent={!!transparent}
      />
    </View>
  );
});
