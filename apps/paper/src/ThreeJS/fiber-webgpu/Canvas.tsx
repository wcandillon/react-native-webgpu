import * as React from "react";
import * as THREE from "three/webgpu";
import {
  StyleSheet,
  View,
  type ViewProps,
  type ViewStyle,
  type GestureResponderHandlers,
  type GestureResponderEvent,
  PanResponder,
  type LayoutChangeEvent,
  PixelRatio,
} from "react-native";
import { useContextBridge, FiberProvider } from "its-fine";
import {
  createRoot,
  extend,
  ReconcilerRoot,
  RenderProps,
  unmountComponentAtNode,
} from "../../../build/react-three-fiber/packages/fiber/src/core";
import { createPointerEvents } from "../../../build/react-three-fiber/packages/fiber/src/web/events";
import {
  Block,
  ErrorBoundary,
  SetBlock,
  useMutableCallback,
} from "../../../build/react-three-fiber/packages/fiber/src/core/utils";
import {
  CanvasContext,
  Canvas as WebGPUCanvas,
  useCanvasEffect,
} from "react-native-wgpu";
import {
  RootState,
  Size,
} from "../../../build/react-three-fiber/packages/fiber/src/core/store";

export interface CanvasProps
  extends Omit<RenderProps<HTMLCanvasElement>, "size" | "dpr">,
    ViewProps {
  children: React.ReactNode;
  style?: ViewStyle;
}

export interface Props extends CanvasProps {}

/**
 * A native canvas which accepts threejs elements as children.
 * @see https://docs.pmnd.rs/react-three-fiber/api/canvas
 */
const CanvasImpl = /*#__PURE__*/ React.forwardRef<View, Props>(
  (
    {
      children,
      style,
      gl,
      events = createPointerEvents,
      shadows,
      linear,
      flat,
      legacy,
      orthographic,
      frameloop,
      performance,
      raycaster,
      camera,
      scene,
      onPointerMissed,
      onCreated,
      ...props
    },
    forwardedRef
  ) => {
    // Create a known catalogue of Threejs-native elements
    // This will include the entire THREE namespace by default, users can extend
    // their own elements by using the createRoot API instead
    React.useMemo(() => {
      extend(THREE);
    }, []);

    const Bridge = useContextBridge();

    const [{ width, height, top, left }, setSize] = React.useState<Size>({
      width: 0,
      height: 0,
      top: 0,
      left: 0,
    });
    const [canvas, setCanvas] = React.useState<HTMLCanvasElement | null>(null);
    const [context, setContext] = React.useState<CanvasContext>();

    const [bind, setBind] = React.useState<GestureResponderHandlers>();
    React.useImperativeHandle(forwardedRef, () => viewRef.current);

    const handlePointerMissed = useMutableCallback(onPointerMissed);
    const [block, setBlock] = React.useState<SetBlock>(false);
    const [error, setError] = React.useState<Error | undefined>(undefined);

    const canvasRef = useCanvasEffect(async () => {
      const context = canvasRef.current!.getContext("webgpu")!;
      setContext(context);

      onContextCreate(context);
    });

    // Suspend this component if block is a promise (2nd run)
    if (block) throw block;
    // Throw exception outwards if anything within canvas throws
    if (error) throw error;

    const viewRef = React.useRef<View>(null!);
    const root = React.useRef<ReconcilerRoot<HTMLCanvasElement>>(null!);

    const onLayout = React.useCallback((e: LayoutChangeEvent) => {
      const { width, height, x, y } = e.nativeEvent.layout;
      setSize({ width, height, top: y, left: x });
    }, []);

    const onContextCreate = React.useCallback((context: GPUCanvasContext) => {
      const listeners = new Map<string, EventListener[]>();

      const gpuCanvas = context.canvas as HTMLCanvasElement;

      const baseCanvas = {
        style: {},
        width: gpuCanvas.width * PixelRatio.get(),
        height: gpuCanvas.height * PixelRatio.get(),
        clientWidth: gpuCanvas.clientWidth,
        clientHeight: gpuCanvas.clientHeight,
        getContext: (_: any) => {
          return context;
        },
        addEventListener(type: string, listener: EventListener) {
          let callbacks = listeners.get(type);
          if (!callbacks) {
            callbacks = [];
            listeners.set(type, callbacks);
          }

          callbacks.push(listener);
        },
        removeEventListener(type: string, listener: EventListener) {
          const callbacks = listeners.get(type);
          if (callbacks) {
            const index = callbacks.indexOf(listener);
            if (index !== -1) callbacks.splice(index, 1);
          }
        },
        dispatchEvent(event: Event) {
          Object.assign(event, { target: this });

          const callbacks = listeners.get(event.type);
          if (callbacks) {
            for (const callback of callbacks) {
              callback(event);
            }
          }
        },
        setPointerCapture() {
          // TODO
        },
        releasePointerCapture() {
          // TODO
        },
      } as unknown as HTMLCanvasElement;

      // TODO: this is wrong but necessary to trick controls
      // @ts-ignore
      baseCanvas.ownerDocument = baseCanvas;
      baseCanvas.getRootNode = () => baseCanvas;

      root.current = createRoot<HTMLCanvasElement>(baseCanvas);
      setCanvas(baseCanvas);

      function handleTouch(
        gestureEvent: GestureResponderEvent,
        type: string
      ): true {
        gestureEvent.persist();

        baseCanvas.dispatchEvent(
          Object.assign(gestureEvent.nativeEvent, {
            type,
            offsetX: gestureEvent.nativeEvent.locationX,
            offsetY: gestureEvent.nativeEvent.locationY,
            pointerType: "touch",
          }) as unknown as Event
        );

        return true;
      }

      const responder = PanResponder.create({
        onStartShouldSetPanResponder: () => true,
        onMoveShouldSetPanResponder: () => true,
        onMoveShouldSetPanResponderCapture: () => true,
        onPanResponderTerminationRequest: () => true,
        onStartShouldSetPanResponderCapture: (e) =>
          handleTouch(e, "pointercapture"),
        onPanResponderStart: (e) => handleTouch(e, "pointerdown"),
        onPanResponderMove: (e) => handleTouch(e, "pointermove"),
        onPanResponderEnd: (e, state) => {
          handleTouch(e, "pointerup");
          if (Math.hypot(state.dx, state.dy) < 20) handleTouch(e, "click");
        },
        onPanResponderRelease: (e) => handleTouch(e, "pointerleave"),
        onPanResponderTerminate: (e) => handleTouch(e, "lostpointercapture"),
        onPanResponderReject: (e) => handleTouch(e, "lostpointercapture"),
      });
      setBind(responder.panHandlers);
    }, []);

    if (root.current) {
      root.current.configure({
        gl: (canvas: HTMLCanvasElement) => {
          return new THREE.WebGPURenderer({ canvas });
        },
        events,
        shadows,
        linear,
        flat,
        legacy,
        orthographic,
        frameloop,
        performance,
        raycaster,
        camera,
        scene,
        dpr: PixelRatio.get(),
        size: { width, height, top, left },
        // Pass mutable reference to onPointerMissed so it's free to update
        onPointerMissed: (...args) => handlePointerMissed.current?.(...args),
        // Overwrite onCreated to apply RN bindings
        onCreated: async (state: RootState) => {
          try {
            // Initialize WebGPU renderer??
            await state.gl.init();

            // Bind render to RN bridge
            const renderFrame = state.gl.render.bind(state.gl);
            state.gl.render = (scene: THREE.Scene, camera: THREE.Camera) => {
              renderFrame(scene, camera);
              context?.present();
            };

            // Ensure frames don't start rendering until context has been created
            //state.set({ frameloop: "always" });
          } catch (e) {
            console.error("Failed to initialize renderer", e);
          }

          return onCreated?.(state);
        },
      });

      root.current.render(
        <Bridge>
          <ErrorBoundary set={setError}>
            <React.Suspense fallback={<Block set={setBlock} />}>
              {children}
            </React.Suspense>
          </ErrorBoundary>
        </Bridge>
      );
    }

    React.useEffect(() => {
      return () => {
        if (canvas != null) {
          unmountComponentAtNode(canvas!);
        }
      };
    }, [canvas]);

    return (
      <View
        {...props}
        onLayout={onLayout}
        ref={viewRef}
        style={{ flex: 1, ...style }}
        {...bind}
      >
        <WebGPUCanvas ref={canvasRef} style={StyleSheet.absoluteFill} />
      </View>
    );
  }
);

/**
 * A native canvas which accepts threejs elements as children.
 * @see https://docs.pmnd.rs/react-three-fiber/api/canvas
 */
export const Canvas = React.forwardRef<View, Props>(
  function CanvasWrapper(props, ref) {
    return (
      <FiberProvider>
        <CanvasImpl {...props} ref={ref} />
      </FiberProvider>
    );
  }
);
