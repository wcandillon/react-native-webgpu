/// <reference types="@webgpu/types" />

import type { NativeCanvas, RNCanvasContext } from "./types";

export * from "./main";

declare global {
  interface Navigator {
    gpu: GPU;
  }

  // If a non-DOM env, this augment global with navigator
  // eslint-disable-next-line @typescript-eslint/ban-ts-comment
  // @ts-ignore: Ignore if 'Navigator' doesn't exist

  var navigator: Navigator;

  var RNWebGPU: {
    gpu: GPU;
    fabric: boolean;
    getNativeSurface: (contextId: number) => NativeCanvas;
    MakeWebGPUCanvasContext: (
      contextId: number,
      width: number,
      height: number,
    ) => RNCanvasContext;
    DecodeToUTF8: (buffer: NodeJS.ArrayBufferView | ArrayBuffer) => string;
    createImageBitmap: typeof createImageBitmap;
  };
}
