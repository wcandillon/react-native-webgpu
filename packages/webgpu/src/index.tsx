/// <reference types="@webgpu/types" />

import type { NativeCanvas } from "./types";

export * from "./main";

declare global {
  interface Navigator {
    gpu: GPU;
  }

  var navigator: Navigator;

  var RNWebGPU: {
    gpu: GPU;
    fabric: boolean;
    getNativeSurface: (contextId: number) => NativeCanvas;
    MakeWebGPUCanvasContext: (
      contextId: number,
      width: number,
      height: number,
    ) => GPUCanvasContext;
    DecodeToUTF8: (buffer: NodeJS.ArrayBufferView | ArrayBuffer) => string;
    createImageBitmap: typeof createImageBitmap;
  };

  // Extend createImageBitmap to accept ArrayBuffer/TypedArray (encoded image bytes)
  function createImageBitmap(
    image: ArrayBuffer | ArrayBufferView,
  ): Promise<ImageBitmap>;
}
