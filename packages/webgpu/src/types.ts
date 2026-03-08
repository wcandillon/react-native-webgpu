type SurfacePointer = bigint;

/** Dawn-specific debug toggle configuration (non-standard WebGPU extension) */
export interface DawnTogglesDescriptor {
  /**
   * List of toggle names to enable.
   * e.g. 'dump_shaders', 'use_user_defined_labels_in_backend',
   *      'disable_symbol_renaming', 'emit_hlsl_debug_symbols'
   */
  enable?: string[];
  /** List of toggle names to disable. */
  disable?: string[];
}

declare global {
  interface GPUDeviceDescriptor {
    /** Dawn-specific toggles. Ignored in non-Dawn environments. */
    dawnToggles?: DawnTogglesDescriptor;
  }
}

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
