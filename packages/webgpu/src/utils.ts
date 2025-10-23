export function contextIdToId(_contextId: number): string {
  throw new Error("contextIdToId is not implemented on the native platform");
}

export const { fabric } = RNWebGPU;

export function getNativeSurface(contextId: number) {
  return RNWebGPU.getNativeSurface(contextId);
}

export function makeWebGPUCanvasContext(
  contextId: number,
  width: number,
  height: number,
) {
  return RNWebGPU.MakeWebGPUCanvasContext(contextId, width, height);
}
