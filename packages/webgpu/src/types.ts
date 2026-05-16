type SurfacePointer = bigint;

export interface NativeCanvas {
  surface: SurfacePointer;
  width: number;
  height: number;
  clientWidth: number;
  clientHeight: number;
}

export interface CanvasRef {
  getContextId: () => number;
  getContext(contextName: "webgpu"): GPUCanvasContext | null;
  getNativeSurface: () => NativeCanvas;
  whenReady: (callback: () => void) => void;
}
