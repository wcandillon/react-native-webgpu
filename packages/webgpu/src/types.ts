type SurfacePointer = bigint;

/** Dawn固有のデバッグトグル設定 (WebGPU仕様外) */
export interface DawnTogglesDescriptor {
  /**
   * 有効化するトグル名の配列
   * 例: 'dump_shaders', 'use_user_defined_labels_in_backend',
   *     'disable_symbol_renaming', 'emit_hlsl_debug_symbols'
   */
  enable?: string[];
  /** 無効化するトグル名の配列 */
  disable?: string[];
}

declare global {
  interface GPUDeviceDescriptor {
    /** Dawn固有のトグル設定。非Dawn環境では無視される。 */
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
