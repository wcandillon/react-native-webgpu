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
