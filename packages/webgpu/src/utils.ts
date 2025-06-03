import { NativeCanvas, RNCanvasContext } from "./types";

//Only used on the web
export function contextIdToId(___: number): string {
  throw new Error("Should not call contextIdToId on native!");
}

export function getNativeSurface(contextId: number): NativeCanvas {
  return RNWebGPU.getNativeSurface(contextId);
}

export function MakeWebGPUCanvasContext(
  contextId: number,
  width: number,
  height: number,
): RNCanvasContext {
  return RNWebGPU.MakeWebGPUCanvasContext(contextId, width, height);
}

export function fabricIsEnabled(): boolean {
  return RNWebGPU.fabric;
}
